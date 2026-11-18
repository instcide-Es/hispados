/* Cabecera de la biblioteca de C para compilar NetSurf (terceros/netsurf) en
 * NAVEGA2.EXE, el Navegador completo de Hispa-486 (w32/surf, docs/486/NAVEGADOR2.md):
 * lo de MSVCRT.DLL con sus estructuras, y lo que falta en w32/surf/apoyo.c.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef HS_NETINET_IN_H
#define HS_NETINET_IN_H
#include <stdint.h>
#include <sys/socket.h>
typedef uint32_t in_addr_t;
typedef uint16_t in_port_t;
struct in_addr { in_addr_t s_addr; };
struct in6_addr { uint8_t s6_addr[16]; };
struct sockaddr_in { short sin_family; in_port_t sin_port; struct in_addr sin_addr; char sin_zero[8]; };
#define INADDR_ANY 0
#define IPPROTO_TCP 6
#endif
