/* wifi.h - WIFI.BIN, el servicio Wi-Fi de Hispa-Pentium: las llamadas al
 * núcleo, las órdenes que le piden los programas (la llamada 100) y las que
 * él le pide a RED.BIN (la 90), y lo que contesta. Los mismos números están
 * en castel/nucleo32/red.cas (el núcleo), rbase.cas y rwifi.cas (RED.BIN) y
 * uredwifi.cas (la ventana REDWIFI.BIN): si cambia uno, los otros.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef HISPA_WIFI_PENTIUM_H
#define HISPA_WIFI_PENTIUM_H

/* ---- las llamadas al sistema (int 0x80; usuario.cas y red.cas) ---- */
#define SC_DORMIR 3
#define SC_TICS 4
#define SC_ABRIR 7		/* EBX nombre, ECX modo (0 leer, 2 crear, 3 al final) -> manejador */
#define SC_LEER 8
#define SC_CERRAR 9
#define SC_LARGO 13
#define SC_ARGUMENTOS 14
#define SC_DATO 15
#define SC_ESCRIBIR_ARCHIVO 18
#define SC_BORRAR 21
#define SC_CORTE 29
#define SC_SERIE 61
#define SC_RED 90		/* pedir a RED.BIN */
#define SC_APUNTARSE 91		/* (WIFI.BIN) IOPL 3, su memoria de DMA y sus registros */
#define SC_SIGUIENTE 92
#define SC_CONTESTAR 93
#define SC_DMA 94		/* EBX páginas seguidas (WIFI.BIN: hasta 1024), ECX -> su dirección física */
#define SC_REGISTROS 95
#define SC_ESTADO 96		/* (WIFI.BIN) EBX estado | barras << 8, ECX señal (dBm), EDX el aparato PCI */
#define DATO_HZ 9
#define ES_ERROR(r) ((unsigned)(r) >= 0xFFFFFFE0u)

/* ---- lo que dice WIFI.BIN al núcleo con la 96 (para el icono) ---- */
#define WE_SIN_TARJETA 1	/* no hay tarjeta, o no se ha podido encender */
#define WE_SIN_CONECTAR 2	/* la radio en marcha, sin red */
#define WE_CONECTANDO 3		/* buscando, autenticando, asociando, claves */
#define WE_CONECTADA 4

/* ---- las órdenes a RED.BIN (rbase.cas: O_WIFI_...) ---- */
#define O_WIFI_PONER 40		/* soy la tarjeta Wi-Fi: cab[1] y cab[2] la MAC; los octetos, su nombre */
#define O_WIFI_CAMBIO 41	/* cab[1] bit 0 enlace; cab[2] señal; los octetos, las tramas que han llegado
				   ([2 octetos de largo][la trama]...) -> las que hay que enviar, igual;
				   en cab[1], bit 0: que se pare WIFI.BIN (Cable) */
#define O_WIFI_QUITAR 42	/* ya no hay tarjeta Wi-Fi */

/* ---- las órdenes que atiende WIFI.BIN (la llamada 100) ---- */
#define WO_ESTADO 1		/* -> struct w_estado_ui */
#define WO_BUSCAR 2		/* buscar redes (unos 2 s) */
#define WO_REDES 3		/* -> struct w_red_ui[], cab[0] cuántas */
#define WO_CONECTAR 4		/* los octetos: el nombre (cab[1] octetos) y detrás la contraseña (cab[2]); cab[3]: bit 0,
				   conectar automáticamente; bit 1, no recordarla; bits 8-15, la seguridad + 1 (0: la que
				   haya) -> 0, o W_R_... */
#define WO_DESCONECTAR 5
#define WO_OLVIDAR 6		/* los octetos: el nombre */
#define WO_REGISTRO 7		/* -> lo último que ha apuntado */
#define WO_SIMCORTE 9		/* (CORTES) solo con la radio simulada: el punto de acceso desaparece cab[1]
				 * segundos; con cab[2] = 1, DHCP da otra IP (192.168.77.11); con 2, la .10 */
#define WO_AUTO 8		/* los octetos: el nombre; cab[1] = 1/0: conectar automáticamente */

/* lo que contesta WO_CONECTAR (cab[0]) */
#define W_R_BIEN 0
#define W_R_FALTA_CLAVE 1	/* es WPA2 y no se sabe su contraseña */
#define W_R_NO_ADMITIDA 2	/* WEP, WPA (TKIP), WPA2-Empresa, WPA3... */
#define W_R_NO_SE_VE 3		/* no está en la lista */
#define W_R_SIN_RADIO 4
#define W_R_CLAVE_CORTA 5	/* WPA2: de 8 a 63 letras */
#define W_R_GEMELA 6		/* abierta con el nombre de una WPA2 recordada: puede ser falsa; no se usa */

#define SIN_WIFI 0xFFFFFFE1u	/* (el núcleo) no hay servicio Wi-Fi */
#define AVISO_ACABO 0xFFFF
#define MAX_DATOS 8192

/* lo que da la llamada 92 (Pedido_red de red.cas); detrás, los octetos */
struct pedido {
	unsigned hueco, serie, tarea, cabe, largo;
	unsigned relleno[3];
	unsigned cab[8];
};
/* lo que se le da a la 93 (Respuesta_red) */
struct respuesta {
	unsigned serie, datos;
	unsigned cab[8];
};

/* WO_ESTADO (Estado_wifi en uredwifi.cas: todo de 4 en 4) */
struct w_estado_ui {
	unsigned estado, fallo, canal;	/* W_PARADO... de nucleo.h; W_F_...; 1 a 13 */
	int senal;			/* dBm */
	unsigned seguridad, barriendo, nredes, calculando, autom, radio;
	unsigned tx, rx, reconexiones;
	unsigned ssid_n;
	unsigned char ssid[36];
	unsigned char mac[8];
	char tarjeta[48];
	char aviso[96];			/* lo último que hay que decirle a quien mira (en la página 437) */
};
/* WO_REDES (Red_wifi en uredwifi.cas) */
#define WR_RECORDADA 1
#define WR_AUTO 2
#define WR_CONECTADA 4
struct w_red_ui {
	unsigned canal;
	int senal;
	unsigned seguridad, marcas, ssid_n;
	unsigned char ssid[36];
	unsigned char bssid[8];
};

static inline unsigned sc(unsigned n, unsigned a, unsigned b, unsigned c)
{
	unsigned r;

	__asm__ __volatile__("int $0x80" : "=a"(r) : "a"(n), "b"(a), "c"(b), "d"(c) : "memory");
	return r;
}
#endif
