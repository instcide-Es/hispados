/* Cabecera de la biblioteca de C para compilar NetSurf (terceros/netsurf) en
 * NAVEGA2.EXE, el Navegador completo de Hispa-486 (w32/surf, docs/486/NAVEGADOR2.md):
 * lo de MSVCRT.DLL con sus estructuras, y lo que falta en w32/surf/apoyo.c.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef HS_ERRNO_H
#define HS_ERRNO_H
int *_errno(void);
#define errno (*_errno())
#define EPERM 1
#define ENOENT 2
#define EINTR 4
#define EIO 5
#define EBADF 9
#define EAGAIN 11
#define ENOMEM 12
#define EACCES 13
#define EEXIST 17
#define ENOTDIR 20
#define EISDIR 21
#define EINVAL 22
#define ENFILE 23
#define EMFILE 24
#define ENOSPC 28
#define ERANGE 34
#define EILSEQ 42
#define E2BIG 7
#define ENOSYS 40
#define ENAMETOOLONG 38
#define ENOTEMPTY 41
#define EWOULDBLOCK EAGAIN
#define ETIMEDOUT 138
#define EAFNOSUPPORT 102
#define ENOTSUP 129
#define EOVERFLOW 132
#endif
