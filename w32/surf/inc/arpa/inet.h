/* Cabecera de la biblioteca de C para compilar NetSurf (terceros/netsurf) en
 * NAVEGA2.EXE, el Navegador completo de Hispa-486 (w32/surf, docs/486/NAVEGADOR2.md):
 * lo de MSVCRT.DLL con sus estructuras, y lo que falta en w32/surf/apoyo.c.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef HS_ARPA_INET_H
#define HS_ARPA_INET_H
#include <netinet/in.h>
const char *inet_ntop(int, const void *, char *, socklen_t);
unsigned short htons(unsigned short);
unsigned int htonl(unsigned int);
unsigned short ntohs(unsigned short);
unsigned int ntohl(unsigned int);
#endif
