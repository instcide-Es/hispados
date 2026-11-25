/* bt.h - (BLUETOOTH) el Bluetooth de Hispa-Pentium (BT.BIN): lo común del
 * servicio (servicio.c), su pila (hci.c, l2cap.c, intel.c) y su ventana
 * (ventana.c). docs/PENTIUM.md, «El Bluetooth».
 *
 * La pila no sabe dónde corre: lo que necesita de fuera (mandar y recibir
 * por el USB, el reloj, el registro, leer un firmware, darle al núcleo lo
 * que teclea un teclado) son las funciones «bt_...» y «tr_...» de abajo, que
 * pone servicio.c en Hispa-Pentium y tests/bt/prueba.c en Linux (contra el
 * modelo de controlador de tests/bt/modelo.c).
 *
 * Lo que se ha tomado de BlueZ y de Linux (btusb, btintel; GPL-2.0 o
 * posterior, como esto) son los hechos —los números de las órdenes y de los
 * sucesos, el orden de los pasos, cómo se llama cada firmware—, cada uno con
 * la función de Linux 6.6 de la que sale; el código es nuestro.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef HISPA_BT_H
#define HISPA_BT_H
#include <stddef.h>
#include <stdint.h>

typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;
typedef int8_t s8;
typedef int32_t s32;

#define BT_VERSION "0.1"

/* ---- las llamadas del núcleo (usbbt.cas) ---- */
#define SC_BT_APUNTARSE 240	/* (solo BT.BIN) -> 0 */
#define SC_BT_INFO 241		/* EBX el adaptador, ECX adónde (struct bt_info) -> cuántos hay */
#define SC_BT_ENVIAR 242	/* EBX la clase (TR_...), ECX los octetos, EDX cuántos -> los que han ido */
#define SC_BT_RECIBIR 243	/* EBX adónde, ECX cuánto cabe, EDX los tics (bit 31: sin dormir) -> n | de << 16 */
#define SC_BT_HID 244		/* EBX op | ranura << 8 | clase << 16, ECX los octetos, EDX cuántos */
#define SC_BT_ESTADO 245	/* EBX el estado (BTE_...), ECX un texto (hasta 80) */
#define SC_BT_PEDIR 246		/* (cualquiera) EBX el pedido, ECX la respuesta, EDX cuánto cabe -> los octetos */
#define SC_BT_SIGUIENTE 247	/* (BT.BIN) EBX adónde, ECX los tics de espera -> el largo (0: ninguno) */
#define SC_BT_CONTESTAR 248	/* (BT.BIN) EBX los octetos, ECX cuántos */

#define TR_ORDEN 1		/* una orden HCI, por el extremo 0 (control) */
#define TR_ACL 2		/* datos ACL, por el extremo a granel de salida */
#define TR_ORDEN_GRANEL 3	/* (Intel, en el cargador) una orden por el de granel */
#define DE_INTERRUPCION 1	/* lo que llega: los sucesos */
#define DE_GRANEL 2		/* los datos ACL (o, con un Intel en su cargador, sucesos) */

#define HID_ALTA 1
#define HID_INFORME 2
#define HID_BAJA 3
#define HID_TECLADO 1
#define HID_RATON 2

/* el estado, para el núcleo (la 245: el Administrador de dispositivos) */
#define BTE_NADA 0
#define BTE_SIN_ADAPTADOR 1
#define BTE_APAGADO 2
#define BTE_ENCENDIDO 3
#define BTE_FALLO 4

struct bt_info {			/* lo que da la 241 de un adaptador */
	u16 fabricante, producto, version, aparato;
	u32 cambios;			/* sube cada vez que se enchufa o se quita uno */
	u8 velocidad, hc, interfaz, resto;
};

/* ---- lo que pide la ventana al servicio (la 246) ---- */
#define BO_ESTADO 1
#define BO_ENCENDER 2
#define BO_APAGAR 3
#define BO_BUSCAR 4
#define BO_EMPAREJAR 5		/* dir[6] */
#define BO_OLVIDAR 6		/* dir[6] */
#define BO_RESPONDER 7		/* a: 1 sí, 0 no (la comparación numérica) */
#define BO_CANCELAR 8
#define BO_CONECTAR 9		/* dir[6]: reconectar uno emparejado */
#define BO_DESCONECTAR 10
struct bt_pedido {
	u32 admin;			/* lo pone el núcleo: 1 si quien pide es administrador */
	u32 largo;			/* (lo pone el núcleo) */
	u32 op, a;
	u8 dir[6], resto[2];
};

#define MAX_LISTA 20
#define BL_EMPAREJADO 1
#define BL_CONECTADO 2
#define BL_VISTO 4
#define BL_LE 8			/* visto por Bluetooth de bajo consumo (BLE) */
#define BL_HID_LE 16		/* anuncia el servicio HID por GATT (BLE: la etapa 2 b) */
#define BL_CONECTANDO 32
struct bt_fila {
	u8 dir[6];
	s8 rssi;
	u8 banderas;
	u32 clase;			/* la clase del aparato (BR/EDR) */
	u16 aspecto;			/* (BLE) la apariencia */
	u8 hid;				/* HID_TECLADO, HID_RATON (o los dos: 3) */
	u8 resto;
	char nombre[48];		/* (UTF-8) */
};
/* el emparejamiento en curso (lo que enseña la ventana, con el código en grande) */
#define EM_NADA 0
#define EM_EN_CURSO 1		/* sin nada que hacer: esperando */
#define EM_TECLEAR 2		/* escriba el código en el teclado y pulse Intro */
#define EM_COMPARAR 3		/* ¿sale el mismo número en el otro aparato? Sí / No */
#define EM_PIN 4		/* (emparejamiento antiguo) escriba el PIN en el teclado */
#define EM_BIEN 5
#define EM_MAL 6
struct bt_estado {
	u32 estado;			/* BTE_... */
	u32 buscando;			/* 1 mientras busca */
	u8 dir[6], resto[2];		/* la del adaptador */
	char adaptador[64];		/* quién es («Intel 8087:0A2A (Wireless 7265)») */
	char aviso[96];			/* lo último que ha pasado */
	u32 em;				/* EM_... */
	u32 codigo, tecleados;		/* el código (6 cifras) y las que se llevan tecleadas */
	char em_nombre[48];		/* con quién */
	u32 n;
	struct bt_fila fila[MAX_LISTA];
};

#ifndef BT_SOLO_TIPOS
/* ---- lo que pone la plataforma ---- */
u32 bt_tics(void);			/* centésimas de segundo */
int tr_envia(int clase, const u8 *d, int n);	/* -1 si no ha ido */
int tr_recibe(u8 *d, int cabe, int *de, u32 espera);	/* los octetos (0: nada; -1: sin adaptador) */
void bt_log(const char *f, ...);	/* a BT.LOG (y al puerto serie) */
int bt_firmware(const char *nombre, u8 *d, u32 cabe);	/* lo lee de \FIRMWARE; -1 (y dice dónde ha mirado) */
void bt_hid(int op, int ranura, int clase, const u8 *d, int n);
void bt_azar(void *d, u32 n);
void bt_guarda(void);			/* los emparejados han cambiado: a BT.INI */
void bt_estado_cambia(void);		/* para el núcleo (la 245) */
void bt_a_437(char *d, const u8 *s, int n, int cabe);	/* un nombre que llega (UTF-8), como lo enseñan las ventanas */
#ifdef HL_PENTIUM_BT
#define formatear bt_formatear
#endif
int formatear(char *d, int cabe, const char *p, ...);
void *memcpy(void *, const void *, size_t);
void *memmove(void *, const void *, size_t);
void *memset(void *, int, size_t);
int memcmp(const void *, const void *, size_t);
size_t strlen(const char *);

/* ---- la pila (hci.c) ---- */
#define MAX_APARATOS 24
#define MAX_DESC 512
struct aparato {
	u8 usado, dir[6];
	u8 le, le_tipo;			/* visto por BLE (y si su dirección es pública o al azar) */
	u32 clase;
	u16 aspecto;
	s8 rssi;
	u8 visto, hid_le;
	u8 sin_nombre;			/* (falta pedirle el nombre) */
	u8 psrm;			/* (de la búsqueda: para conectar antes) */
	u16 reloj;
	char nombre[48];
	/* emparejado */
	u8 emparejado, tipo_clave, clave[16];
	u8 es_hid, hid_clase, boot;	/* lo que dice su SDP */
	u16 desc_n;
	u8 desc[MAX_DESC];		/* su descriptor de informe (HID) */
	/* conectado */
	u16 handle;
	u8 conectado, cifrado, entrante, estado;
	u8 quiere_emparejar;		/* lo ha pedido la ventana */
	int r_tec, r_rat;		/* las ranuras del núcleo (HID) del teclado y del ratón, o -1 */
	u8 protocolo;			/* 0: el de arranque; 1: el de informes (con su descriptor) */
	u32 desde;			/* el tic en que empezó lo de ahora */
	u8 intentos;
};
extern struct aparato aparatos[MAX_APARATOS];

/* los pasos de una conexión */
#define A_NADA 0
#define A_CONECTANDO 1		/* Create_Connection */
#define A_AUTENTICANDO 2	/* Authentication_Requested (y el emparejamiento) */
#define A_CIFRANDO 3
#define A_SDP 4			/* preguntando su descriptor HID */
#define A_HID 5			/* abriendo los canales HID (0x11 y 0x13) */
#define A_LISTO 6		/* teclea o mueve */

struct bt_pila {
	int listo, encendido, buscando, buscando_le;
	u8 dir[6];
	u8 version, lmp;
	u16 revision, lmp_sub, fabricante_lmp;
	u8 rasgos[8];			/* LMP features (página 0) */
	u16 acl_mtu, acl_max;		/* Read_Buffer_Size */
	int acl_libres;
	int ordenes_libres;		/* Num_HCI_Command_Packets */
	u16 vid, pid;
	char nombre[64];
	char aviso[96];
	int intel_cargador;		/* un Intel en su cargador: los sucesos llegan también por granel */
	u32 fin_busqueda;
	/* el emparejamiento en curso */
	int em, em_aparato;
	u32 em_codigo, em_tecleados, em_desde;
	int simulado;			/* (las pruebas) */
};
extern struct bt_pila bt;

int bt_arranca(u16 vid, u16 pid);	/* reset, dirección, versión... 0 si vale */
void bt_vuelta(void);			/* lo que haya llegado, y los relojes */
void bt_apaga(void);
int bt_enciende(void);
void bt_busca(void);
int bt_empareja(const u8 *dir);
int bt_olvida(const u8 *dir);
void bt_responde(int si);
void bt_cancela(void);
int bt_conecta(const u8 *dir);
void bt_reconecta_todos(void);		/* al arrancar: los emparejados */
void bt_lista(struct bt_estado *e);
struct aparato *bt_aparato(const u8 *dir, int crear);
const char *bt_dir_corta(const u8 *dir);	/* «xx:xx:xx:xx:3F:21» (los 2 últimos: el registro) */
const char *bt_dir_texto(const u8 *dir);	/* entera */
const char *bt_clase_texto(u32 clase, u16 aspecto);

/* hci.c: mandar */
int hci_manda(u16 op, const void *p, int n);	/* sin esperar */
int hci_espera(u16 op, const void *p, int n, u8 *r, int cabe, u32 tics);	/* espera su Command Complete (los parámetros a r): el largo, o -1 */
int hci_acl(u16 handle, const u8 *d, int n);	/* un paquete L2CAP entero (lo parte) */
int hci_recoge(u32 espera);		/* lo que haya llegado (bit 31: sin dormir): 1 si algo */

/* l2cap.c */
void l2_conectado(struct aparato *a);
void l2_desconectado(struct aparato *a);
void l2_cifrado(struct aparato *a);		/* ya se puede abrir lo que esperaba */
void l2_llega(struct aparato *a, const u8 *d, int n);	/* un paquete L2CAP entero */
void l2_empieza_hid(struct aparato *a);	/* SDP (si hace falta) y luego los canales HID */
void l2_vuelta(void);
void l2_reinicia(void);
void l2_corta_hid(struct aparato *a, int unplug);

/* intel.c: lo de los adaptadores de Intel antes del reset (0: listo; -1: no se puede) */
int intel_prepara(u16 pid);

/* el registro de los paquetes (BT.LOG con «Registro=TODO»), sin claves */
extern int bt_registro_todo;
#endif
#endif
