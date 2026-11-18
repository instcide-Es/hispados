/* Cabecera de la biblioteca de C para compilar NetSurf (terceros/netsurf) en
 * NAVEGA2.EXE, el Navegador completo de Hispa-486 (w32/surf, docs/486/NAVEGADOR2.md):
 * lo de MSVCRT.DLL con sus estructuras, y lo que falta en w32/surf/apoyo.c.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef HS_UNISTD_H
#define HS_UNISTD_H
#include <sys/types.h>
#define F_OK 0
#define W_OK 2
#define R_OK 4
#define X_OK 0
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
int access(const char *, int); int unlink(const char *); int rmdir(const char *); int close(int);
ssize_t read(int, void *, size_t); ssize_t write(int, const void *, size_t); off_t lseek(int, off_t, int);
char *getcwd(char *, size_t); int chdir(const char *); int isatty(int);
pid_t getpid(void); unsigned sleep(unsigned); int usleep(unsigned);
int ftruncate(int, off_t); long sysconf(int); int fsync(int);
uid_t getuid(void);
int gethostname(char *, size_t);
#define _SC_PAGESIZE 30
#endif
