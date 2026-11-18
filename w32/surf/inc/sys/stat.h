/* Cabecera de la biblioteca de C para compilar NetSurf (terceros/netsurf) en
 * NAVEGA2.EXE, el Navegador completo de Hispa-486 (w32/surf, docs/486/NAVEGADOR2.md):
 * lo de MSVCRT.DLL con sus estructuras, y lo que falta en w32/surf/apoyo.c.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef HS_SYS_STAT_H
#define HS_SYS_STAT_H
#include <sys/types.h>
#include <time.h>
/* la de MSVCRT (_stat32) */
struct stat { dev_t st_dev; ino_t st_ino; unsigned short st_mode; short st_nlink; short st_uid; short st_gid;
              dev_t st_rdev; long st_size; time_t st_atime; time_t st_mtime; time_t st_ctime; };
#define S_IFMT 0xF000
#define S_IFDIR 0x4000
#define S_IFCHR 0x2000
#define S_IFREG 0x8000
#define S_IFIFO 0x1000
#define S_IFLNK 0xA000
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#define S_ISLNK(m) 0
#define S_IRUSR 0400
#define S_IWUSR 0200
#define S_IXUSR 0100
#define S_IRWXU 0700
#define S_IRGRP 040
#define S_IWGRP 020
#define S_IXGRP 010
#define S_IRWXG 070
#define S_IROTH 04
#define S_IWOTH 02
#define S_IXOTH 01
#define S_IRWXO 07
int stat(const char *, struct stat *);
int fstat(int, struct stat *);
int lstat(const char *, struct stat *);
int mkdir(const char *, mode_t);
int chmod(const char *, mode_t);
#endif
