/* Cabecera de la biblioteca de C para compilar NetSurf (terceros/netsurf) en
 * NAVEGA2.EXE, el Navegador completo de Hispa-486 (w32/surf, docs/486/NAVEGADOR2.md):
 * lo de MSVCRT.DLL con sus estructuras, y lo que falta en w32/surf/apoyo.c.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef HS_SYS_SOCKET_H
#define HS_SYS_SOCKET_H
#define AF_UNSPEC 0
#define AF_INET 2
#define AF_INET6 23
#define SOCK_STREAM 1
typedef int socklen_t;
struct sockaddr { unsigned short sa_family; char sa_data[14]; };
#endif
