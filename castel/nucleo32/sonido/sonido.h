/* sonido.h - (SONIDO) el servicio de sonido de Hispa-Pentium (SONIDO.BIN):
 * lo común de sus partes (servicio.c, mezcla.c, hda.c, ac97.c, sb16.c,
 * ventana.c). Las estructuras que se pasan con el núcleo son las de
 * nucleo32/sonido.cas (Estado_sonido, Info_canal, Orden_sonido y
 * Para_servicio): si cambia una, la otra.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef SONIDO_H
#define SONIDO_H
#include "hispa.h"

typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

/* ---- las llamadas del sonido (sonido.cas) ---- */
#define SC_ABRIR_CANAL 190
#define SC_DAR 191
#define SC_CANAL 192
#define SC_SONIDO 193
#define SC_TOCAR_WAV 194
#define SC_APUNTARSE 195
#define SC_DMA 196
#define SC_REGISTROS 197
#define SC_INTERCAMBIO 198
#define SC_SACAR 199

#define TARJETA_NINGUNA 0
#define TARJETA_SB16 2
#define TARJETA_AC97 3
#define TARJETA_HDA 4

#define SS_INICIO 0
#define SS_APAGAR 1
#define SS_AVISO 2
#define SS_ERROR 3
#define SS_VOLUMEN 4
#define N_SISTEMA 5

#define C_CERRAR 0
#define C_VOLUMEN 1
#define C_PAUSA 2
#define C_VACIAR 3
#define C_EN_COLA 4
#define C_ESPERAR 5
#define C_TOCADOS 6
#define C_CERRAR_AL_ACABAR 7

#define G_ESTADO 0
#define G_VOLUMEN 1
#define G_SILENCIO 2
#define G_SISTEMA 3
#define G_PROBAR 4
#define G_SONIDOS 5
#define G_SUBIR 6
#define G_VOLUMEN_CANAL 7
#define G_CANALES 8

#define O_SISTEMA 1
#define O_PROBAR 2
#define O_WAV 3
#define O_PARAR_VOZ 4
#define O_ACABO 5
#define O_CALLAR 6

#define MAX_CANALES 8
#define MAX_ORDENES 16

struct estado_sonido {			/* 240 octetos */
	u32 version, tarjeta, hz, pci, dma_isa, salida, detecta, auriculares;
	u32 volumen, silencio, sistema, voces, cambios, puertos, memoria, irq;
	char nombre[48];
	char codec[48];
	char detalle[80];
};
struct info_canal {			/* 48 octetos */
	u32 dueno, hz, canales, bits, volumen, pausa, en_cola, tocados;
	char nombre[16];
};
struct orden_sonido {			/* 128 octetos */
	u32 clase, a, b, relleno;
	char ruta[112];
};
struct para_servicio {
	u32 volumen, silencio, sistema, cambios, n_ordenes;
	u32 relleno[3];
	struct info_canal canales[MAX_CANALES];
	struct orden_sonido ordenes[MAX_ORDENES];
};

/* la salida (estado_sonido.salida) */
#define SALIDA_ALTAVOZ 0
#define SALIDA_AURICULARES 1
#define SALIDA_LINEA 2

/* ---- los puertos (el servicio tiene IOPL 3) ---- */
static inline u8 inb(u16 p) { u8 v; __asm__ __volatile__("inb %1, %0" : "=a"(v) : "Nd"(p)); return v; }
static inline u16 inw(u16 p) { u16 v; __asm__ __volatile__("inw %1, %0" : "=a"(v) : "Nd"(p)); return v; }
static inline u32 inl(u16 p) { u32 v; __asm__ __volatile__("inl %1, %0" : "=a"(v) : "Nd"(p)); return v; }
static inline void outb(u16 p, u8 v) { __asm__ __volatile__("outb %0, %1" : : "a"(v), "Nd"(p)); }
static inline void outw(u16 p, u16 v) { __asm__ __volatile__("outw %0, %1" : : "a"(v), "Nd"(p)); }
static inline void outl(u16 p, u32 v) { __asm__ __volatile__("outl %0, %1" : : "a"(v), "Nd"(p)); }
static inline u32 cli_(void) { u32 f; __asm__ __volatile__("pushf; pop %0; cli" : "=r"(f) : : "memory"); return f; }
static inline void sti_(u32 f) { if (f & 0x200) __asm__ __volatile__("sti" : : : "memory"); }

/* ---- servicio.c ---- */
/* el PCI: «a» es bus << 16 | aparato << 11 | función << 8 */
u32 pci_lee(u32 a, u32 r, int n);
void pci_escribe(u32 a, u32 r, u32 v, int n);
u32 pci_busca(u32 clase, u32 mascara, u32 desde);	/* 0xFFFFFFFF: no hay */
void *registros(u32 fisica, u32 octetos);	/* los registros de la tarjeta, a la vista; 0 si no */
void *dma(u32 paginas, u32 *fisica, int baja);	/* memoria física seguida; 0 si no */
void espera_us(u32 us);
u32 divide64(u64 n, u32 d);		/* n / d (sin la de libgcc), si cabe en 32 bits */			/* espera ocupada (con el puerto 80h) */
extern struct estado_sonido estado;
extern int estado_cambiado;
extern char ini_texto[2048];		/* \HISPA\SONIDO.INI, tal cual */

/* ---- la tarjeta (hda.c, ac97.c, sb16.c) ---- */
#define ANILLO (64 * 1024)		/* lo que da la vuelta la DMA (16 páginas) */
struct tarjeta {
	int (*empezar)(void);		/* la DMA en marcha, dando vueltas al anillo */
	u32 (*posicion)(void);		/* por dónde lee la tarjeta (octetos del anillo) */
	void (*cada_vez)(void);		/* en cada vuelta del servicio */
	void (*vigilar)(void);		/* cada medio segundo: los auriculares */
	void (*callar)(void);		/* para apagar: parada y sin ruido */
};
extern u8 *anillo;			/* el anillo de la DMA, a la vista */
extern u32 anillo_fis;
extern u32 hz_salida;
int hda_buscar(struct tarjeta *t);	/* 1 si hay y está en marcha */
int ac97_buscar(struct tarjeta *t);
int sb16_buscar(struct tarjeta *t);

/* ---- mezcla.c ---- */
#define MAX_VOCES 16
void mezcla_prepara(void);
void mezcla(i16 *d, u32 tramas);	/* las voces sumadas, en estéreo de 16 bits */
int mezcla_activa(void);		/* ¿suena algo? */
u32 mezcla_voces(void);
void mezcla_canales(struct para_servicio *p);	/* los canales del núcleo, al día */
int voz_memoria(const u8 *wav, u32 n, u32 id, u32 dueno, int lados, int volumen);
int voz_archivo(const char *ruta, u32 id, u32 dueno);
int voz_tono(u32 hz, u32 ms, int lados, int volumen);
void voz_parar(u32 id);
void voces_de(u32 dueno);		/* para las de esa tarea */
extern u32 vol_general;			/* 0..100 */
extern int en_silencio;
/* lo que dice la cabecera de un .WAV: formato, canales, frecuencia, bits y
 * dónde están los datos (0 si no vale) */
u32 wav_cabecera(const u8 *d, u32 n, u32 *hz, u32 *canales, u32 *bits, u32 *largo);

#endif
