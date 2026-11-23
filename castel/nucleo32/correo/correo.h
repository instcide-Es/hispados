/* correo.h - Correo: lo de todos (las cuentas, las carpetas, la ficha de
 * cada mensaje en el índice) y las piezas del núcleo: el IMAP (imap.c), el
 * SMTP (smtp.c), el POP3 (pop3.c), la autoconfiguración (autocfg.c), las
 * cuentas y los ajustes (cuentas.c), lo guardado (almacen.c) y la libreta
 * (libreta.c). C99 portable (so.h).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef CORREO_CORREO_H
#define CORREO_CORREO_H
#include "so.h"
#include "red.h"
#include "mime.h"

#define CORREO_VERSION "1.0.3"
#define CORREO_PROGRAMA "Correo 1.0.3 (Hispa-Pentium)"

/* =============================================================================
 *  Las cuentas (cuentas.c)
 * ============================================================================= */
#define CT_IMAP 0
#define CT_POP3 1
#define MAX_CUENTAS 8

typedef struct {
	int num;			/* 1..MAX_CUENTAS: la carpeta Cn del almacén */
	char nombre[64];		/* el de la lista: «ana@prueba.test» o el que se le ponga */
	char correo[128];		/* la dirección */
	char visible[96];		/* el nombre que ven los demás («Ana Pérez») */
	char usuario[128];		/* el de entrar (casi siempre, la dirección) */
	int tipo;			/* CT_IMAP o CT_POP3 */
	char entrada[128];		/* el servidor de entrada (IMAP o POP3) */
	u32 puerto_entrada;
	int modo_entrada;		/* SO_TLS, SO_STARTTLS_IMAP/POP3 o SO_PLANO */
	char salida[128];		/* el SMTP */
	u32 puerto_salida;
	int modo_salida;		/* SO_TLS, SO_STARTTLS_SMTP o SO_PLANO */
	int sin_cifrar;			/* «permitir sin cifrar» (solo servidores locales) */
	int guardar_clave;		/* la contraseña, guardada (en el almacén, que solo lee CORREO.BIN) */
	char clave[128];		/* (en la memoria; guardada solo si guardar_clave) */
	char firma[600];		/* (UTF-8, renglones con \n) */
	int dejar_en_servidor;		/* (POP3) no borrar lo bajado */
	u32 max_kb;			/* no bajar solo los mensajes de más de tantos KB (0: todos) */
	u32 avisar;			/* avisar de lo nuevo (en la bandeja) */
	char enviados[200], borradores[200], papelera[200];	/* (IMAP) las carpetas, por su nombre del servidor */
} Cuenta;

typedef struct {
	u32 max_mensaje_kb;		/* lo más grande que se baja entero (de entrada, 25 MB) */
	u32 max_almacen_mb;		/* lo más que ocupa lo guardado de cada carpeta (0: sin tope) */
	u32 cada_min;			/* mirar si hay correo nuevo cada tantos minutos (sin IDLE) */
	int html;			/* enseñar el HTML (si no, el texto) */
	int hilos;			/* la lista, por hilos */
	int avisar;			/* el aviso de correo nuevo en la bandeja */
	int imagenes_conocidos;		/* (no se usa: las imágenes de fuera, siempre a mano) */
	char ispdb[200];		/* la base de datos de autoconfiguración (la de Mozilla) */
	int orden_col, orden_baja;	/* la columna por la que se ordena y si va de mayor a menor */
} Ajustes;

extern Cuenta cuentas[MAX_CUENTAS];
extern int n_cuentas;
extern Ajustes ajustes;
extern char dir_datos[200];

int cuentas_lee(void);
int cuentas_guarda(void);
void ajustes_lee(void);
int ajustes_guarda(void);
/* ¿es un servidor de la red de casa? (10.x, 192.168.x, 172.16-31.x, 127.x,
 * .local, .lan, localhost): solo a esos se les deja ir sin cifrar */
int servidor_local(const char *nombre);
/* la cuenta nueva: su número (la carpeta), o -1 si no caben más */
int cuenta_nueva(Cuenta *c);
int cuenta_quita(int i);

/* =============================================================================
 *  La autoconfiguración (autocfg.c)
 * ============================================================================= */
typedef struct {
	int encontrada;		/* 0: nada; 1: de la lista de aquí; 2: ISPDB/autoconfig; 3: SRV; 4: probando */
	char de_donde[160];	/* «la base de datos de Mozilla», «probando imap.dominio»... */
	Cuenta c;
	char aviso[400];	/* lo que hay que saber de ese proveedor (contraseña de aplicación...) */
} Autocfg;
/* busca los servidores de una dirección (sin conectar todavía a la cuenta):
 * la lista de los de siempre, autoconfig del dominio, la ISPDB de Mozilla,
 * los SRV y, si no, prueba imap., mail. y smtp. del dominio */
int autocfg_busca(const char *correo, Autocfg *a, void (*paso)(void *u, const char *que), void *u);
/* (las pruebas) lo que se saca de un XML de autoconfiguración */
int autocfg_xml(const char *xml, u32 n, const char *correo, Cuenta *c);

/* =============================================================================
 *  Las carpetas y las fichas
 * ============================================================================= */
#define CM_NOSELECT 0x0001	/* no se puede abrir (solo lleva otras) */
#define CM_HIJOS 0x0002
#define CM_ENTRADA 0x0010
#define CM_ENVIADOS 0x0020
#define CM_BORRADORES 0x0040
#define CM_PAPELERA 0x0080
#define CM_BASURA 0x0100
#define CM_ARCHIVO 0x0200
#define CM_TODOS 0x0400
#define CM_DESTACADOS 0x0800
#define CM_LOCAL 0x1000		/* (POP3) la carpeta es solo de aquí */
#define MAX_CARPETAS 200

typedef struct {
	char nombre[200];	/* el que se ve («Asociación/Cuotas»; «Bandeja de entrada») */
	char crudo[256];	/* el del servidor (UTF-7 modificado: «Asociaci&APM-n/Cuotas») */
	char delim;
	u32 marcas;		/* CM_... */
	u32 total, no_leidos, uidnext, uidvalidity;
	int num;		/* su carpeta en el almacén: Fnnn */
	int nivel;		/* por cuántos delimitadores va */
} Carpeta;

/* las banderas de un mensaje */
#define B_LEIDO 0x0001
#define B_DESTACADO 0x0002
#define B_RESPONDIDO 0x0004
#define B_BORRADO 0x0008
#define B_BORRADOR 0x0010
#define B_REENVIADO 0x0020
#define B_ADJUNTO 0x0040	/* (por sus cabeceras: lleva adjuntos, seguramente) */
#define B_NUEVO 0x0080		/* ha llegado ahora */
#define B_PENDIENTE 0x0100	/* (sin conexión) cambiado aquí, falta decírselo al servidor */
#define B_SUPLANTA 0x0200	/* el remitente no cuadra (mime_suplantacion) */
#define B_BAJADO 0x0400		/* el mensaje entero está guardado */
#define B_GRANDE 0x0800		/* no se ha bajado por grande (max_kb) */

/* la ficha de un mensaje: lo de la lista (192 octetos en INDICE.IDX) */
typedef struct {
	u32 uid;
	u32 fecha;		/* segundos desde 1970, UTC */
	u32 tam;
	u32 banderas;
	u32 pos, largo;		/* en CUERPOS.DAT (largo 0: no se ha bajado) */
	u32 hilo;		/* el resumen de su Message-ID */
	u32 padre;		/* el de lo que contesta (In-Reply-To, o lo último de References) */
	char de[40];		/* el nombre (o la dirección) del remitente; en Enviados, el destinatario */
	char dir[40];		/* su dirección */
	char asunto[80];
} Ficha;

/* un resumen de 32 bits de un texto (los Message-ID) */
u32 resumen32(const char *t, u32 n);
/* copia un UTF-8 en «d» sin pasar de cabe-1 octetos ni partir una letra */
void copia_utf8(char *d, u32 cabe, const char *t, u32 n);

/* la ficha de las cabeceras de un mensaje (las de HEADER.FIELDS, o el
 * mensaje entero) */
void ficha_de_cabeceras(Ficha *f, const u8 *cab, u32 n, int de_enviados);

/* =============================================================================
 *  El IMAP (imap.c)
 * ============================================================================= */
#define IC_IDLE 0x0001
#define IC_UIDPLUS 0x0002
#define IC_MOVE 0x0004
#define IC_SPECIAL_USE 0x0008
#define IC_LITERALPLUS 0x0010
#define IC_STARTTLS 0x0020
#define IC_LOGINDISABLED 0x0040
#define IC_AUTH_PLAIN 0x0080
#define IC_UNSELECT 0x0100
#define IC_IMAP4REV1 0x0200

typedef struct {
	Con con;
	u32 etiqueta;
	u32 caps;
	int dentro;		/* ha entrado (LOGIN) */
	char sel[256];		/* la carpeta abierta (cruda) */
	int solo_leer;
	u32 uidvalidity, uidnext, exists;
	int en_idle;
	u32 nuevo_exists;	/* (IDLE, NOOP) lo último que ha dicho EXISTS */
	int hubo_expunge;
	int hubo_banderas;
	u32 copyuid;		/* lo de [APPENDUID v uid] o [COPYUID] */
	char error[300];	/* el último, en español (con lo que ha dicho el servidor) */
	/* la respuesta que se está leyendo */
	u8 *r;
	u32 rn, rcabe;
	u32 max_literal;
} Imap;

int imap_conecta(Imap *m, const Cuenta *c, const char *clave);
void imap_cierra(Imap *m);
int imap_carpetas(Imap *m, Carpeta *v, int max, int con_status);
int imap_status(Imap *m, Carpeta *c);
int imap_selecciona(Imap *m, const char *crudo, int solo_leer);
/* UID SEARCH: los UID (en memoria nueva), de menor a mayor */
int imap_busca_uids(Imap *m, const char *criterio, const char *texto, u32 **uids, u32 *n);
/* las fichas de esos UID (en tramos): cada() por cada una que llega */
int imap_fichas(Imap *m, const u32 *uids, u32 n, int de_enviados, void (*cada)(void *u, Ficha *f), void *u);
/* las banderas de los UID de desde a hasta */
int imap_banderas(Imap *m, u32 desde, u32 hasta, void (*cada)(void *u, u32 uid, u32 banderas), void *u);
/* el mensaje entero (en memoria nueva) */
int imap_mensaje(Imap *m, u32 uid, u8 **d, u32 *n, u32 max, int marcar_leido);
int imap_marca(Imap *m, const u32 *uids, u32 n, u32 banderas, int poner);
int imap_mueve(Imap *m, const u32 *uids, u32 n, const char *destino);
int imap_borra(Imap *m, const u32 *uids, u32 n);	/* \Deleted y fuera (UID EXPUNGE si se puede) */
int imap_anade(Imap *m, const char *carpeta, const u8 *d, u32 n, u32 banderas, u32 *uid);
int imap_noop(Imap *m);
int imap_idle_empieza(Imap *m);
/* mira si ha dicho algo (EXISTS, EXPUNGE, FETCH): 1 si sí, 0 si no, <0 si se ha cortado */
int imap_idle_mira(Imap *m, u32 espera_ms);
int imap_idle_acaba(Imap *m);
/* (las pruebas) el analizador de respuestas, con lo que diga un servidor */
int imap_prueba_respuestas(const u8 *d, u32 n);
/* el UTF-7 modificado de los nombres de las carpetas (RFC 3501 5.1.3) */
void utf7m_a_utf8(const char *t, char *d, u32 cabe);
void utf8_a_utf7m(const char *t, char *d, u32 cabe);

/* =============================================================================
 *  El SMTP (smtp.c) y el POP3 (pop3.c)
 * ============================================================================= */
/* envía un mensaje: el remitente, los destinatarios (las direcciones,
 * separadas por comas) y el mensaje entero. 0 si va; si no, en «error» */
int smtp_envia(const Cuenta *c, const char *clave, const char *de, const char *destinos, const u8 *d, u32 n,
	       char *error, int cabe);

typedef struct {
	Con con;
	char error[300];
	u32 n;			/* cuántos hay */
} Pop3;
int pop3_conecta(Pop3 *p, const Cuenta *c, const char *clave);
/* el UIDL de cada uno (número y su UIDL) */
int pop3_uidl(Pop3 *p, void (*cada)(void *u, u32 num, const char *uidl, u32 tam), void *u);
int pop3_mensaje(Pop3 *p, u32 num, u8 **d, u32 *n, u32 max);
int pop3_borra(Pop3 *p, u32 num);
void pop3_cierra(Pop3 *p);

/* =============================================================================
 *  Lo guardado (almacen.c): en la carpeta del correo, Cn (una por cuenta) y,
 *  dentro, Fnnn (una por carpeta del servidor) con INDICE.IDX (las fichas,
 *  por orden de UID) y CUERPOS.DAT (los mensajes bajados, uno detrás de
 *  otro). Así se lee sin red, y un buzón de 50.000 mensajes son 50.000
 *  fichas de 192 octetos (9 MB) y no 50.000 archivos en una carpeta FAT.
 * ============================================================================= */
typedef struct {
	int cuenta, carpeta;	/* sus números (Cn, Fnnn) */
	u32 uidvalidity;
	u32 uidnext;		/* (lo que dijo el servidor la última vez) */
	Ficha *f;		/* por orden de UID */
	u32 n, cabe;
	u32 sucio_desde;	/* lo que hay que escribir de INDICE.IDX (0xFFFFFFFF: nada) */
	int reescribir;		/* (se ha quitado o metido algo en medio) */
	u32 cuerpos;		/* lo que mide CUERPOS.DAT */
	u32 muertos;		/* lo que ya no es de nadie en CUERPOS.DAT */
} Buzon;

void alm_ruta(char *d, u32 cabe, int cuenta, int carpeta, const char *nombre);
int alm_carpetas_lee(int cuenta, Carpeta *v, int max);
int alm_carpetas_guarda(int cuenta, const Carpeta *v, int n);
/* el número de almacén de una carpeta (uno nuevo si no tiene) */
void alm_numera(Carpeta *v, int n);
int buzon_abre(Buzon *b, int cuenta, int carpeta);
int buzon_guarda(Buzon *b);
void buzon_cierra(Buzon *b);
void buzon_vacia(Buzon *b, u32 uidvalidity);
Ficha *buzon_busca(Buzon *b, u32 uid);
u32 buzon_sitio(Buzon *b, u32 uid);	/* dónde iría (búsqueda por la mitad) */
Ficha *buzon_pon(Buzon *b, const Ficha *f);
void buzon_quita(Buzon *b, u32 uid);
void buzon_cambia(Buzon *b, Ficha *f);	/* (se han cambiado sus banderas) */
u32 buzon_max_uid(const Buzon *b);
int buzon_guarda_cuerpo(Buzon *b, Ficha *f, const u8 *d, u32 n);
u8 *buzon_cuerpo(Buzon *b, const Ficha *f, u32 *n);
/* rehace CUERPOS.DAT sin lo muerto (y, con tope, sin los más viejos) */
int buzon_compacta(Buzon *b, u32 tope_octetos);

/* =============================================================================
 *  La libreta de direcciones (libreta.c): LIBRETA.VCF en la carpeta del correo
 * ============================================================================= */
#include "vcard.h"
#define MAX_CONTACTOS 2000
extern Contacto *libreta;
extern int n_libreta;
int libreta_lee(void);
int libreta_guarda(void);
/* lo apunta (si no está) o le suma un uso */
void libreta_usa(const char *nombre, const char *dir);
/* las que empiezan por «t» (en el nombre o en la dirección, sin tildes): cuántas */
int libreta_completa(const char *t, int *v, int max);
int libreta_importa(const char *ruta);
int libreta_exporta(const char *ruta);

#endif
