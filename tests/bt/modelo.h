/* modelo.h - (BLUETOOTH) el modelo de adaptador Bluetooth (HCI por USB) con
 * aparatos de mentira al otro lado (tests/bt/modelo.c): lo usan la prueba
 * en el anfitrión (tests/bt/prueba.c, con la pila de BT.BIN) y el adaptador
 * USB de mentira para QEMU (tests/bt/usbredir_bt.c, por usb-redir).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef MODELO_BT_H
#define MODELO_BT_H
#include <stdint.h>

/* las clases de adaptador */
#define MOD_GENERICO 0		/* uno cualquiera (0A12:0001, como un CSR) */
#define MOD_INTEL_ROM 1		/* 8087:0A2A (la 7265: sin firmware) */
#define MOD_INTEL_CARGADOR 2	/* 8087:0AAA en su cargador (pide ibt-17-16-1.sfi) */

/* los aparatos del otro lado */
#define R_TECLADO 0		/* «Teclado de prueba»: SSP, solo teclado (el código se teclea) */
#define R_RATON 1		/* «Ratón de prueba»: SSP sin nada (Just Works), sin protocolo de arranque */
#define R_TELEFONO 2		/* «Teléfono de prueba»: comparación numérica; no es HID */
#define R_ANTIGUO 3		/* «Teclado antiguo»: sin SSP (el PIN) */
#define R_BLE 4			/* «Ratón BLE»: solo anuncia (HID por GATT) */
#define N_REMOTOS 5

void mod_inicia(int clase, const char *carpeta_fw);	/* (la carpeta donde está el .sfi de mentira, para comprobarlo) */
void mod_recibe(int clase, const uint8_t *d, int n);	/* lo que manda el anfitrión (TR_ORDEN, TR_ACL, TR_ORDEN_GRANEL) */
int mod_saca(uint8_t *d, int cabe, int *de, int paquete_int, int paquete_granel);	/* lo siguiente: un trozo (como el USB) */
void mod_tic(uint32_t t);				/* el reloj (centésimas) */
uint16_t mod_vid(void), mod_pid(void);

/* lo que hace «la persona» con los aparatos */
void mod_emparejable(int r, int si);			/* el aparato, en modo de emparejar (se deja ver) */
int mod_teclea_codigo(int r, uint32_t codigo);		/* teclea el código en el teclado y pulsa Intro */
void mod_acepta(int r, int si);				/* (el teléfono) «sí, es el mismo número» */
int mod_tecla(int r, uint8_t mods, uint8_t uso);	/* pulsa (uso 0: suelta todo); 0 si ha ido */
int mod_mueve(int r, int dx, int dy, int botones, int rueda);
void mod_despierta(int r);				/* (encendido) se conecta él solo, como al pulsar una tecla */
void mod_apaga(int r);					/* se apaga (o se va lejos) */
int mod_conectado(int r);
int mod_protocolo(int r);				/* el que le han puesto (0 arranque, 1 informes, -1 ninguno) */
int mod_emparejado(int r);
extern int mod_fallos;					/* lo que el anfitrión ha hecho mal */
extern int mod_firmware_bien;				/* (Intel) el firmware ha llegado entero y en orden */
extern int mod_ddc;					/* (Intel) las partes de su configuración que han llegado */
extern int mod_parche;					/* (Intel ROM) órdenes del parche */
extern uint32_t mod_ordenes;				/* las órdenes HCI que han llegado */

/* (QEMU, usbredir_bt.c) «la persona» hace sola lo que toca: teclea el código
 * de emparejar en el teclado, y cuando el teclado y el ratón están listos,
 * teclea «texto» y mueve el ratón; si el aparato ya estaba emparejado (de
 * un arranque anterior: mod_lee), se despierta solo a los pocos segundos */
void mod_auto(const char *texto_teclado);
int mod_en_cargador(void);
int mod_guarda(const char *archivo);			/* las claves de los aparatos */
int mod_lee(const char *archivo);
extern int mod_cambio_claves;				/* (sube cuando hay que guardarlas) */
extern int mod_tecleado, mod_movido;			/* lo que ha mandado «la persona» */
#endif
