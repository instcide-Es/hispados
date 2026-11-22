/* mime.h - Correo: los mensajes por dentro (RFC 5322, MIME: RFC 2045-2049,
 * RFC 2047, RFC 2231), los juegos de caracteres, las fechas y las
 * direcciones; y hacer un mensaje para enviarlo. C99 portable (so.h).
 *
 * Todo lo que llega de un servidor se trata como hostil: cada cosa con su
 * tope (MIME_MAX_*), nada de recursión sin tope, nada se lee fuera de lo que
 * se ha dado. Lo malformado se lee como se pueda (como Thunderbird), no se
 * para el programa.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef CORREO_MIME_H
#define CORREO_MIME_H
#include "so.h"

#define MIME_MAX_PARTES 512	/* partes de un mensaje (lo de más no se mira) */
#define MIME_MAX_NIVEL 24	/* multipart dentro de multipart */
#define MIME_MAX_CABECERAS 256	/* cabeceras de una parte */
#define MIME_MAX_MENSAJE (64u << 20)	/* lo más grande que se analiza */

/* ---- los juegos de caracteres (juegos.c) ---- */
/* ¿lo sabe pasar a UTF-8? (us-ascii, utf-8, iso-8859-1, -2, -3, -4, -5,
 * -7, -9, -13, -15, -16, windows-1250 a 1258, ibm437/cp437, cp850,
 * koi8-r, koi8-u, macintosh; con sus alias: latin1, cp1252, ansi_x3.4-1968,
 * utf8...). Sin mayúsculas ni minúsculas */
int jc_conocido(const char *juego);
/* de «juego» a UTF-8, en memoria nueva (so_pide, con un 0 detrás). Un juego
 * desconocido o vacío: si es UTF-8 válido, tal cual; si no, como
 * windows-1252. Lo que no es válido en UTF-8, U+FFFD */
char *jc_a_utf8(const char *juego, const u8 *d, u32 n, u32 *largo);
/* ¿es UTF-8 válido? (sin sobrelargos ni sustitutos) */
int utf8_valido(const u8 *d, u32 n);
/* la letra siguiente de un UTF-8 (U+FFFD si no vale); avanza *i */
u32 utf8_letra(const u8 *d, u32 n, u32 *i);
/* una letra a UTF-8: los octetos (1 a 4) */
int utf8_pon(u32 letra, char *d);
/* cuántas letras lleva */
u32 utf8_cuenta(const char *t, u32 n);
/* UTF-8 a la página 437 de Hispa (tools/hispa437.py: con À Á Â Ã È Ê Ë Ì Í
 * Î Ï Ò Ó Ô Õ Ù Ú Û ã õ Œ œ © ® € …); lo que no tiene, lo más parecido
 * (“ ” -> ", – — -> -, ‘ ’ -> ', š -> s...) o «?». Da los octetos escritos
 * (sin pasar de cabe-1; acaba en 0) */
u32 utf8_a_437(const char *t, u32 n, char *d, u32 cabe);
/* una letra de la 437 de Hispa (lo que teclea el usuario) a Unicode */
u32 letra_de_437(u8 c);
/* minúsculas sin tildes, para buscar («Ñandú» -> «nandu»): en memoria nueva */
char *utf8_plano(const char *t, u32 n, u32 *largo);

/* ---- base64 y quoted-printable ---- */
/* descodificar (lo malo se salta): los octetos, en memoria nueva */
u8 *b64_descodifica(const u8 *d, u32 n, u32 *largo);
u8 *qp_descodifica(const u8 *d, u32 n, u32 *largo, int en_cabecera);
/* codificar, con renglones de 76 y CRLF (b64) / con «=» al final de los
 * renglones partidos (qp); en memoria nueva */
char *b64_codifica(const u8 *d, u32 n, u32 *largo, int con_renglones);
char *qp_codifica(const u8 *d, u32 n, u32 *largo);

/* ---- las cabeceras ---- */
/* RFC 2047 (=?juego?B?...?= y =?juego?Q?...?=, también partidas y pegadas,
 * con lenguaje: =?utf-8*es?Q?...?=) a UTF-8; los octetos de 8 bits sueltos
 * (de servidores que no codifican), como UTF-8 si lo son y si no como
 * windows-1252. En memoria nueva */
char *rfc2047_descodifica(const char *t, u32 n);
/* UTF-8 a RFC 2047 si hace falta (si no, tal cual), para una cabecera que
 * empieza en la columna «col» (para partir a 76): en memoria nueva. Con
 * «es_frase», entrecomilla lo que haga falta (los nombres de las direcciones) */
char *rfc2047_codifica(const char *utf8, u32 col, int es_frase);

/* un valor con parámetros: «attachment; filename*0*=utf-8''Presupuesto%20a;
 * filename*1*=%C3%B1o.pdf». Da el valor de delante (en minúsculas, sin
 * espacios) y busca un parámetro (RFC 2231: con sus trozos *0 *1..., su
 * juego y su lenguaje; y los que vienen en RFC 2047 aunque no se deba; y sin
 * comillas o con ellas). Los dos, en UTF-8 y en memoria nueva; NULL si no
 * está */
char *mime_valor(const char *cabecera, u32 n);
char *mime_parametro(const char *cabecera, u32 n, const char *nombre);

/* ---- las direcciones (RFC 5322: «Ana Pérez <ana@x.es>, "Luis, el de
 * Albinyana" <l@y.es>, grupo: a@b, c@d;», con comentarios y lo mal hecho) ---- */
typedef struct {
	char nombre[128];	/* (UTF-8, ya descodificado; vacío si no lo hay) */
	char dir[256];		/* buzon@dominio (vacío si no se ha podido) */
} Direccion;
/* las direcciones de un valor (crudo, con RFC 2047): cuántas (hasta max) */
int mime_direcciones(const char *valor, u32 n, Direccion *v, int max);
/* ¿parece una dirección válida? (buzon@dominio.tld, sin espacios) */
int mime_dir_valida(const char *dir);
/* «Ana Pérez <ana@x.es>» (con RFC 2047 y comillas si hace falta), para una
 * cabecera: en memoria nueva */
char *mime_dir_cabecera(const Direccion *d);

/* ---- las fechas ---- */
/* RFC 5322 (y lo de antes: dos cifras del año, las zonas con nombre, sin
 * segundos, sin día de la semana, con basura detrás) a segundos desde 1970,
 * UTC; 0 si no se entiende */
i64 mime_fecha(const char *t, u32 n);
/* para la cabecera Date: «Sun, 22 Nov 2026 10:05:00 +0100» */
void mime_fecha_rfc(i64 utc, int zona_min, char *d, int cabe);
/* para la lista: hoy, «10:05»; este año, «22/11 10:05»; si no, «22/11/2025» */
void mime_fecha_corta(i64 utc, int zona_min, i64 ahora, char *d, int cabe);
/* entera: «domingo, 22/11/2026 10:05» */
void mime_fecha_larga(i64 utc, int zona_min, char *d, int cabe);
/* y las de IMAP: «22-Nov-2026 10:05:00 +0100» (INTERNALDATE) a UTC */
i64 mime_fecha_imap(const char *t, u32 n);

/* ---- un mensaje por dentro ---- */
#define COD_NADA 0		/* 7bit, 8bit, binary, o nada */
#define COD_BASE64 1
#define COD_QP 2
#define COD_UUENCODE 3		/* (x-uuencode) */

typedef struct Cabecera {
	char nombre[48];	/* tal cual («Subject») */
	u32 ini, n;		/* el valor, en el mensaje: ya sin el nombre ni los
				 * dos puntos; con los renglones partidos dentro
				 * (mime_desdobla los junta) */
} Cabecera;

typedef struct Parte {
	char tipo[80];		/* «text/plain» (en minúsculas; sin nada: text/plain,
				 * o message/rfc822 en un multipart/digest) */
	char juego[40];		/* charset (minúsculas) */
	char nombre[256];	/* el del adjunto (filename o name; UTF-8; vacío si no) */
	char id[128];		/* Content-ID, sin < > */
	char disposicion[16];	/* «inline», «attachment» o vacío */
	char frontera[80];	/* (multipart) */
	int codificacion;	/* COD_... */
	int flowed, delsp;	/* text/plain; format=flowed (RFC 3676) */
	u32 cab_ini, cab_fin;	/* las cabeceras de la parte, en el mensaje */
	u32 ini, fin;		/* el cuerpo, en el mensaje (aún codificado) */
	int padre;		/* el número de la parte que la lleva, -1 la de arriba */
	int primera, siguiente;	/* los hijos (multipart, message/rfc822): -1 si no hay */
	int nivel;
	int n_cab, cab0;	/* sus cabeceras: Mensaje.cab[cab0 .. cab0+n_cab-1] */
	u32 tam;		/* lo que medirá descodificado (aprox.) */
	int es_adjunto;		/* lo que se ve como adjunto (mime_analiza lo decide) */
} Parte;

typedef struct {
	const u8 *d;		/* el mensaje entero (no es suyo: no se suelta) */
	u32 n;
	Parte *partes;
	int n_partes;
	Cabecera *cab;
	int n_cab;
	int cortado;		/* lo había de más: no se ha mirado todo */
	int malformado;		/* algo estaba mal (aviso para las pruebas) */
} Mensaje;

/* analiza «d» (CRLF o LF): las cabeceras, el árbol de partes (multipart con
 * sus fronteras —también las que no se cierran—, message/rfc822 por dentro,
 * los uuencode sueltos de un texto como adjuntos). NULL si no hay memoria */
Mensaje *mime_analiza(const u8 *d, u32 n);
void mime_libera(Mensaje *m);
/* una cabecera de una parte (0: la de arriba): la primera con ese nombre (sin
 * mayúsculas), con sus renglones juntos («desdoblada»), cruda; en memoria
 * nueva, NULL si no está. «cual»: 0 la primera, 1 la segunda... */
char *mime_cabecera(const Mensaje *m, int parte, const char *nombre, int cual);
/* la misma, ya descodificada (RFC 2047) a UTF-8 */
char *mime_cabecera_texto(const Mensaje *m, int parte, const char *nombre);
/* el cuerpo de una parte, descodificado (base64, qp, uuencode): los octetos
 * (en memoria nueva, con un 0 detrás que no cuenta) */
u8 *mime_cuerpo(const Mensaje *m, int parte, u32 *largo);
/* el de un texto, además en UTF-8 (con su charset; format=flowed ya
 * juntado) */
char *mime_texto(const Mensaje *m, int parte, u32 *largo);
/* lo que se enseña como cuerpo: el text/html o el text/plain de un
 * multipart/alternative (el html si «prefiere_html», si no el texto; si solo
 * hay uno, ese), dentro de mixed y related. -1 si no hay ninguno */
int mime_principal(const Mensaje *m, int prefiere_html);
/* la parte con ese Content-ID (las imágenes de un multipart/related: cid:) */
int mime_por_id(const Mensaje *m, const char *cid);
/* los adjuntos, por orden: el número de parte del adjunto número «k» (desde
 * 0), -1 si no hay tantos. Las imágenes de un related que salen en el HTML no
 * cuentan */
int mime_adjunto(const Mensaje *m, int k);
int mime_n_adjuntos(const Mensaje *m);

/* ---- la suplantación sencilla (seguridad) ----
 * Mira el From (y Reply-To, Sender, Return-Path) de la parte de arriba: si
 * el nombre que se ve lleva una dirección que no es la de verdad («Banco
 * <soporte@banco.es>» <x@malo.example>, «soporte@banco.es <x@malo.example>»),
 * si el Reply-To es de otro dominio, si hay varios From. Da 0 si no ve nada;
 * si no, 1 (aviso) o 2 (grave), con el porqué en «motivo» (en español, UTF-8) */
int mime_suplantacion(const Mensaje *m, char *motivo, int cabe);

/* ---- hacer un mensaje (para enviarlo y para los borradores) ---- */
typedef struct {
	const char *ruta;	/* de dónde leerlo (con so_lee_archivo), o NULL */
	const u8 *datos;	/* o los octetos */
	u32 n;
	const char *nombre;	/* el nombre (UTF-8) */
	const char *tipo;	/* «application/pdf»; NULL: por la extensión */
	const char *id;		/* Content-ID (una imagen del HTML), o NULL */
} Adjunto;

typedef struct {
	Direccion de;
	const char *para, *cc, *cco;	/* como los escribe el usuario: «Ana <a@b.es>, c@d.es» */
	const char *responder_a;	/* Reply-To, o NULL */
	const char *asunto;
	const char *texto;		/* el cuerpo en texto (UTF-8, renglones con \n) */
	const char *html;		/* el mismo en HTML, o NULL (solo texto) */
	const Adjunto *adjuntos;
	int n_adjuntos;
	const char *en_respuesta_a;	/* el Message-ID (con < >) al que contesta, o NULL */
	const char *referencias;	/* References (con < >), o NULL */
	const char *id_mensaje;		/* el Message-ID (con < >); NULL: se hace uno */
	i64 fecha;			/* UTC; 0: so_ahora() */
	int zona;			/* minutos */
	const char *programa;		/* User-Agent («Correo 1.0 (Hispa-Pentium)») */
	int con_cco;			/* poner el Bcc en el mensaje (solo en los borradores) */
} Redactado;
/* el mensaje entero (RFC 5322, CRLF, renglones de menos de 998: el texto en
 * quoted-printable si lleva 8 bits o renglones largos; los adjuntos en
 * base64; los nombres en RFC 2231 y también en RFC 2047, para los programas
 * de antes; multipart/alternative con el HTML; mixed con los adjuntos), en
 * memoria nueva. NULL si no hay memoria, o si un adjunto no se puede leer
 * («error» dice cuál) */
u8 *mime_construye(const Redactado *r, u32 *largo, char *error, int cabe);
/* un Message-ID nuevo: <azar@dominio> */
void mime_nuevo_id(const char *dominio, char *d, int cabe);
/* el tipo de un archivo por su extensión («application/pdf»...; si no se
 * sabe, application/octet-stream) */
const char *mime_tipo_de(const char *nombre);

#endif
