/* archivos.c - Hispa-64, etapa 5 (docs/64/HISPA-64.md, sección 11): las
 * pruebas de los archivos desde un programa (ELF64 estático sin biblioteca,
 * con las llamadas de Linux, que corre igual en Linux).
 *
 *   ARCHIVOS auto      mira las carpetas de «/» (los montajes) y, en cada una
 *                      que tenga HISPA64.PRB (la marca de los discos de
 *                      prueba: nunca se escribe en otro), lee lo que ha
 *                      dejado el anfitrión (LEEME.TXT, PATRON.BIN, nombres
 *                      largos y con acentos) y, si se puede escribir, hace
 *                      la prueba entera en «Prueba de Hispa-64»: crear,
 *                      escribir, leer, O_APPEND, O_TRUNC, pread/pwrite,
 *                      huecos, ftruncate, nombres largos, muchos archivos
 *                      (la carpeta crece), getdents64, renombrar y mover
 *                      (también carpetas), borrar con el archivo abierto,
 *                      rmdir, openat/mkdirat/unlinkat/renameat/newfstatat
 *                      desde una carpeta abierta, chdir y getcwd, los
 *                      errores (ENOENT, EEXIST, EISDIR, ENOTDIR, ENOTEMPTY,
 *                      EXDEV, EROFS, EINVAL, ENAMETOOLONG), fsync; y deja
 *                      ULTIMO.TXT escrito sin fsync (lo vacía el apagado);
 *                      un nombre de 255 letras (lo más: el de 256, no); y,
 *                      si el volumen tiene RAIZFIJA.PRB (las FAT12 y FAT16:
 *                      su raíz no crece), llena la raíz hasta ENOSPC y la
 *                      vacía otra vez;
 *   ARCHIVOS dir RUTA  lo que hay en RUTA, con sus carpetas (como «dir /s»);
 *   ARCHIVOS rapido RUTA  escribe y lee 8 MB (con fsync) y dice cuánto tarda.
 * Lo comprueba después el anfitrión (pruebas.sh, con mtools y fsck.fat).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "sis.h"

enum { S_STAT = 4, S_FSTAT = 5, S_PREAD = 17, S_PWRITE = 18, S_DUP = 32, S_DUP2 = 33, S_FSYNC = 74,
       S_TRUNCATE = 76, S_FTRUNCATE = 77, S_GETCWD = 79, S_CHDIR = 80, S_RENAME = 82, S_MKDIR = 83,
       S_RMDIR = 84, S_UNLINK = 87, S_SYNC = 162, S_GETDENTS64 = 217, S_MKDIRAT = 258,
       S_NEWFSTATAT = 262, S_UNLINKAT = 263, S_RENAMEAT = 264 };
enum { EEXIST_ = 17, EXDEV_ = 18, ENOTDIR_ = 20, EISDIR_ = 21, ENOTEMPTY_ = 39, ENAMETOOLONG_ = 36, EBUSY_ = 16, ENOSPC_ = 28 };
#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2
#define O_CREAT 0x40
#define O_EXCL 0x80
#define O_TRUNC 0x200
#define O_APPEND 0x400
#define O_DIRECTORY 0x10000

struct estado {		/* la «struct stat» de Linux x86-64 */
	u64 dev, ino, nlink;
	unsigned modo, uid, gid, nada;
	u64 rdev;
	i64 tamaño, blksize, bloques;
	u64 atime, atime_ns, mtime, mtime_ns, ctime, ctime_ns;
	i64 res[3];
};

static char buf[1 << 17], buf2[1 << 16];
static char ruta[512], r2[512];

static int igual(const char *a, const char *b) { while (*a && *a == *b) { a++; b++; } return *a == *b; }
static void junta(char *d, const char *a, const char *b)
{
	u64 i = 0, j = 0;
	while (a[i]) { d[i] = a[i]; i++; }
	if (i > 0 && d[i - 1] != '/') d[i++] = '/';
	while (b[j]) d[i++] = b[j++];
	d[i] = 0;
}
static int iguales(const char *a, const char *b, u64 n) { while (n--) if (*a++ != *b++) return 0; return 1; }
static unsigned char patrón(u64 i) { return (unsigned char)(i * 131 + (i >> 8)); }

static i64 abre(const char *r, i64 m) { return sis(S_OPEN, r, m, 0644); }
static i64 tamaño_de(const char *r)
{
	struct estado e;
	if (sis(S_STAT, r, &e, 0) != 0) return -1;
	return e.tamaño;
}
/* escribe un archivo entero (de un texto) */
static int pon_archivo(const char *r, const char *t)
{
	i64 fd = abre(r, O_CREAT | O_WRONLY | O_TRUNC), n = (i64)largo(t);
	if (fd < 0) return (int)fd;
	if (sis(S_WRITE, fd, t, n) != n) { sis(S_CLOSE, fd, 0, 0); return -5; }
	return (int)sis(S_CLOSE, fd, 0, 0);
}
/* ¿dice el archivo exactamente t? */
static int dice(const char *r, const char *t)
{
	i64 fd = abre(r, O_RDONLY), n;
	if (fd < 0) return 0;
	n = sis(S_READ, fd, buf2, sizeof buf2);
	sis(S_CLOSE, fd, 0, 0);
	return n == (i64)largo(t) && iguales(buf2, t, (u64)n);
}
/* cuántas entradas tiene la carpeta (sin «.» ni «..»); y si está «nombre» */
static int cuenta(const char *r, const char *nombre, int *está)
{
	i64 fd = sis(S_OPEN, r, O_RDONLY | O_DIRECTORY, 0), n, k;
	int c = 0, puntos = 0;
	if (está) *está = 0;
	if (fd < 0) return (int)fd;
	while ((n = sis(S_GETDENTS64, fd, buf2, 4096)) > 0) {
		for (k = 0; k < n; ) {
			unsigned short rec = *(unsigned short *)(buf2 + k + 16);
			char *nom = buf2 + k + 19;
			if (igual(nom, ".") || igual(nom, "..")) puntos++;
			else { c++; if (está && nombre && igual(nom, nombre)) *está = 1; }
			k += rec;
		}
	}
	sis(S_CLOSE, fd, 0, 0);
	if (puntos != 2) return -1000;
	return c;
}

/* ---- dir ---- */
static void dir(const char *r, int hondo)
{
	i64 fd = sis(S_OPEN, r, O_RDONLY | O_DIRECTORY, 0), n, k;
	static char sub[8][512];
	if (fd < 0) { pon("  (no se abre "); pon(r); pon(")\n"); return; }
	while ((n = sis(S_GETDENTS64, fd, buf2, 4096)) > 0) {
		for (k = 0; k < n; ) {
			unsigned short rec = *(unsigned short *)(buf2 + k + 16);
			unsigned char tipo = (unsigned char)buf2[k + 18];
			char *nom = buf2 + k + 19;
			i64 sigue = *(i64 *)(buf2 + k + 8);
			int i;
			k += rec;
			if (igual(nom, ".") || igual(nom, "..")) continue;
			for (i = 0; i < hondo + 1; i++) pon("  ");
			pon(nom);
			junta(sub[hondo], r, nom);
			if (tipo == 4) {
				pon("/\n");
				if (hondo < 6) {
					dir(sub[hondo], hondo + 1);
					/* (dir ha pisado buf2: se vuelve a leer desde la siguiente) */
					sis(S_LSEEK, fd, sigue, 0);
					n = sis(S_GETDENTS64, fd, buf2, 4096);
					k = 0;
					if (n <= 0) break;
				}
			} else {
				pon(" ("); pon_n(tamaño_de(sub[hondo])); pon(" octetos)\n");
			}
		}
	}
	sis(S_CLOSE, fd, 0, 0);
}

/* ---- lo que ha dejado el anfitrión ---- */
static void lee_anfitrión(const char *m)
{
	i64 fd, n, i, total;
	int bien = 1;
	junta(ruta, m, "LEEME.TXT");
	mira(dice(ruta, "Hola desde el anfitri\xc3\xb3n: este disco es de prueba.\n"), "LEEME.TXT (lo que dice)");
	junta(ruta, m, "leeme.txt");
	mira(dice(ruta, "Hola desde el anfitri\xc3\xb3n: este disco es de prueba.\n"), "leeme.txt (sin mirar may\xc3\xbasculas)");
	junta(ruta, m, "Carpeta con nombre largo/Canci\xc3\xb3n de cuna \xc3\xb1""and\xc3\xba.txt");
	mira(dice(ruta, "Un nombre largo, con acentos y e\xc3\xb1""e.\n"), "un nombre largo con acentos (UTF-8)");
	junta(ruta, m, "CARPETA CON NOMBRE LARGO/CANCI\xc3\x93N DE CUNA \xc3\x91""AND\xc3\x9a.TXT");
	mira(dice(ruta, "Un nombre largo, con acentos y e\xc3\xb1""e.\n"), "el mismo, en may\xc3\xbasculas (\xc3\x93, \xc3\x91, \xc3\x9a)");
	junta(ruta, m, "PATRON.BIN");
	fd = abre(ruta, O_RDONLY);
	mira(fd >= 0, "PATRON.BIN se abre");
	total = 0;
	if (fd >= 0) {
		while ((n = sis(S_READ, fd, buf, 7777)) > 0) {
			for (i = 0; i < n; i++) if ((unsigned char)buf[i] != patrón((u64)(total + i))) bien = 0;
			total += n;
		}
		/* lseek y pread: un trozo de en medio */
		mira(sis(S_LSEEK, fd, 0, 2) == total, "lseek al final");
		mira(sis6(S_PREAD, fd, (i64)buf, 1000, 123456, 0, 0) == 1000, "pread");
		for (i = 0; i < 1000; i++) if ((unsigned char)buf[i] != patrón((u64)(123456 + i))) bien = 0;
		sis(S_CLOSE, fd, 0, 0);
	}
	mira(bien && total == 300000, "PATRON.BIN: los 300000 octetos, como deben");
	pon("  lee lo del anfitri\xc3\xb3n: LEEME.TXT, un nombre largo con acentos y PATRON.BIN (");
	pon_n(total); pon(" octetos)"); pon(bien ? ", bien\n" : ", MAL\n");
}

/* ---- la prueba entera (en un volumen en el que se puede escribir) ---- */
static void prueba(const char *m)
{
	char d[512], t[600];
	i64 fd, fd2, r, i, n, dfd;
	struct estado e;
	int bien, está, k;
	static char largo_[320];

	junta(d, m, "Prueba de Hispa-64");
	/* (si quedó de antes, fuera: la prueba empieza siempre igual) */
	r = sis(S_MKDIR, d, 0755, 0);
	mira(r == 0 || r == -EEXIST_, "mkdir de una carpeta con nombre largo");
	mira(sis(S_MKDIR, d, 0755, 0) == -EEXIST_, "mkdir otra vez: EEXIST");
	mira(sis(S_STAT, d, &e, 0) == 0 && (e.modo & 0170000) == 0040000, "stat de la carpeta: S_IFDIR");

	/* crear, escribir, leer */
	junta(ruta, d, "hola.txt");
	mira(pon_archivo(ruta, "Hola, mundo\n") == 0, "crear y escribir hola.txt");
	mira(dice(ruta, "Hola, mundo\n"), "leer hola.txt");
	mira(tamaño_de(ruta) == 12, "stat: el tama\xc3\xb1o");
	fd = abre(ruta, O_WRONLY | O_APPEND);
	mira(fd >= 0 && sis(S_WRITE, fd, "segunda l\xc3\xadnea\n", 15) == 15, "O_APPEND");
	mira(sis(S_FSTAT, fd, &e, 0) == 0 && e.tamaño == 27 && (e.modo & 0170000) == 0100000, "fstat tras O_APPEND");
	sis(S_CLOSE, fd, 0, 0);
	fd = abre(ruta, O_RDWR);
	mira(sis6(S_PWRITE, fd, (i64)"MUNDO", 5, 6, 0, 0) == 5, "pwrite");
	mira(sis(S_LSEEK, fd, 0, 1) == 0, "pwrite no mueve la posici\xc3\xb3n");
	sis(S_CLOSE, fd, 0, 0);
	mira(dice(ruta, "Hola, MUNDO\nsegunda l\xc3\xadnea\n"), "lo escrito con pwrite y O_APPEND");
	mira(abre(ruta, O_CREAT | O_EXCL | O_WRONLY) == -EEXIST_, "O_CREAT | O_EXCL de uno que est\xc3\xa1: EEXIST");

	/* uno grande: 1.234.567 octetos, en trozos de 64 KB, y leído entero */
	junta(ruta, d, "grande.bin");
	fd = abre(ruta, O_CREAT | O_WRONLY | O_TRUNC);
	bien = fd >= 0;
	for (n = 0; bien && n < 1234567; n += r) {
		i64 trozo = 1234567 - n > 65536 ? 65536 : 1234567 - n;
		for (i = 0; i < trozo; i++) buf[i] = (char)patrón((u64)(n + i));
		r = sis(S_WRITE, fd, buf, trozo);
		if (r != trozo) bien = 0;
	}
	mira(bien, "escribir grande.bin (1.234.567 octetos)");
	mira(sis(S_FSYNC, fd, 0, 0) == 0, "fsync");
	sis(S_CLOSE, fd, 0, 0);
	/* 100 X en medio, con lseek y write */
	fd = abre(ruta, O_RDWR);
	for (i = 0; i < 100; i++) buf[i] = 'X';
	mira(sis(S_LSEEK, fd, 500000, 0) == 500000 && sis(S_WRITE, fd, buf, 100) == 100, "lseek y escribir en medio");
	sis(S_LSEEK, fd, 0, 0);
	bien = 1;
	for (n = 0; (r = sis(S_READ, fd, buf, 65536)) > 0; n += r)
		for (i = 0; i < r; i++)
			if ((unsigned char)buf[i] != ((n + i >= 500000 && n + i < 500100) ? 'X' : patrón((u64)(n + i)))) bien = 0;
	mira(bien && n == 1234567, "leer grande.bin entero (con las 100 X)");
	sis(S_CLOSE, fd, 0, 0);

	/* un hueco: escribir más allá del final */
	junta(ruta, d, "hueco.bin");
	fd = abre(ruta, O_CREAT | O_RDWR | O_TRUNC);
	mira(sis6(S_PWRITE, fd, (i64)"0123456789", 10, 100000, 0, 0) == 10, "pwrite m\xc3\xa1s all\xc3\xa1 del final");
	mira(sis(S_FSTAT, fd, &e, 0) == 0 && e.tamaño == 100010, "el tama\xc3\xb1o con el hueco");
	bien = sis6(S_PREAD, fd, (i64)buf, 100010, 0, 0, 0) == 100010;
	for (i = 0; i < 100000; i++) if (buf[i] != 0) bien = 0;
	mira(bien && iguales(buf + 100000, "0123456789", 10), "el hueco, a ceros");
	/* ftruncate: m\xc3\xa1s corto y m\xc3\xa1s largo */
	mira(sis(S_FTRUNCATE, fd, 100, 0) == 0 && sis(S_FSTAT, fd, &e, 0) == 0 && e.tamaño == 100, "ftruncate a 100");
	mira(sis(S_FTRUNCATE, fd, 5000, 0) == 0 && sis(S_FSTAT, fd, &e, 0) == 0 && e.tamaño == 5000, "ftruncate a 5000");
	bien = sis6(S_PREAD, fd, (i64)buf, 5000, 0, 0, 0) == 5000;
	for (i = 0; i < 5000; i++) if (buf[i] != 0) bien = 0;
	mira(bien, "lo alargado, a ceros");
	sis(S_CLOSE, fd, 0, 0);
	/* O_TRUNC */
	fd = abre(ruta, O_WRONLY | O_TRUNC);
	mira(fd >= 0 && sis(S_FSTAT, fd, &e, 0) == 0 && e.tamaño == 0, "O_TRUNC");
	sis(S_WRITE, fd, "corto\n", 6);
	sis(S_CLOSE, fd, 0, 0);
	mira(sis(S_TRUNCATE, ruta, 3, 0) == 0 && tamaño_de(ruta) == 3, "truncate (por la ruta)");

	/* un nombre largo con acentos */
	junta(ruta, d, "Un nombre muy largo con acentos \xc3\xa1\xc3\xa9\xc3\xad\xc3\xb3\xc3\xba y e\xc3\xb1""e \xc3\xb1.txt");
	mira(pon_archivo(ruta, "\xc2\xa1Hola!\n") == 0, "crear un nombre largo con acentos");
	junta(ruta, d, "UN NOMBRE MUY LARGO CON ACENTOS \xc3\x81\xc3\x89\xc3\x8d\xc3\x93\xc3\x9a Y E\xc3\x91""E \xc3\x91.TXT");
	mira(dice(ruta, "\xc2\xa1Hola!\n"), "y abrirlo en may\xc3\xbasculas");

	/* muchos archivos: la carpeta crece (y luego se borra la mitad) */
	junta(t, d, "Muchos");
	mira(sis(S_MKDIR, t, 0755, 0) == 0, "mkdir Muchos");
	bien = 1;
	for (k = 0; k < 40; k++) {
		char nom[32] = "archivo n\xc3\xbamero 00.txt";
		nom[16] = (char)('0' + k / 10); nom[17] = (char)('0' + k % 10);
		junta(ruta, t, nom);
		if (pon_archivo(ruta, nom) != 0) bien = 0;
	}
	mira(bien, "40 archivos con nombre largo");
	mira(cuenta(t, "archivo n\xc3\xbamero 39.txt", &está) == 40 && está, "getdents64: los 40 (y \xc2\xab.\xc2\xbb y \xc2\xab..\xc2\xbb)");
	bien = 1;
	for (k = 0; k < 40; k += 2) {
		char nom[32] = "archivo n\xc3\xbamero 00.txt";
		nom[16] = (char)('0' + k / 10); nom[17] = (char)('0' + k % 10);
		junta(ruta, t, nom);
		if (sis(S_UNLINK, ruta, 0, 0) != 0) bien = 0;
	}
	mira(bien, "unlink de 20");
	mira(cuenta(t, "archivo n\xc3\xbamero 38.txt", &está) == 20 && !está, "quedan 20 (sin los pares)");
	junta(ruta, t, "archivo n\xc3\xbamero 07.txt");
	mira(dice(ruta, "archivo n\xc3\xbamero 07.txt"), "y se leen bien");

	/* renombrar y mover */
	junta(ruta, d, "hola.txt");
	junta(r2, d, "adios.txt");
	mira(sis(S_RENAME, ruta, r2, 0) == 0, "rename en la misma carpeta");
	mira(abre(ruta, O_RDONLY) == -ENOENT, "el viejo ya no est\xc3\xa1");
	mira(dice(r2, "Hola, MUNDO\nsegunda l\xc3\xadnea\n"), "el nuevo dice lo mismo");
	junta(t, d, "sub");
	mira(sis(S_MKDIR, t, 0755, 0) == 0, "mkdir sub");
	junta(ruta, t, "Dentro");
	mira(sis(S_MKDIR, ruta, 0755, 0) == 0, "mkdir sub/Dentro");
	junta(ruta, d, "movido.txt");
	mira(pon_archivo(ruta, "me han movido\n") == 0, "crear movido.txt");
	junta(r2, t, "Dentro/movido.txt");
	mira(sis(S_RENAME, ruta, r2, 0) == 0 && dice(r2, "me han movido\n"), "rename a otra carpeta");
	junta(r2, d, "sub2");
	mira(sis(S_RENAME, t, r2, 0) == 0, "rename de una carpeta");
	junta(ruta, d, "sub2/Dentro/movido.txt");
	mira(dice(ruta, "me han movido\n"), "lo de dentro sigue ah\xc3\xad");
	/* una carpeta, a otra (su «..» cambia) y vuelta */
	junta(ruta, d, "sub2/Dentro");
	junta(r2, d, "Muchos/Dentro");
	mira(sis(S_RENAME, ruta, r2, 0) == 0, "mover una carpeta a otra carpeta");
	mira(sis(S_RENAME, r2, ruta, 0) == 0, "y devolverla");
	junta(ruta, d, "sub2");
	junta(r2, d, "sub2/Dentro/dentro de s\xc3\xad");
	mira(sis(S_RENAME, ruta, r2, 0) == -EINVAL, "mover una carpeta dentro de s\xc3\xad misma: EINVAL");
	/* renombrar encima de otro: el otro se va */
	junta(ruta, d, "uno.txt");
	junta(r2, d, "dos.txt");
	pon_archivo(ruta, "uno\n");
	pon_archivo(r2, "dos\n");
	mira(sis(S_RENAME, ruta, r2, 0) == 0 && dice(r2, "uno\n") && abre(ruta, O_RDONLY) == -ENOENT, "rename encima de otro");
	mira(sis(S_UNLINK, r2, 0, 0) == 0, "unlink dos.txt");

	/* borrar un archivo abierto: se sigue leyendo hasta cerrarlo */
	junta(ruta, d, "abierto.txt");
	pon_archivo(ruta, "sigo aqu\xc3\xad\n");
	fd = abre(ruta, O_RDONLY);
	mira(sis(S_UNLINK, ruta, 0, 0) == 0, "unlink de uno abierto");
	mira(abre(ruta, O_RDONLY) == -ENOENT, "ya no se encuentra");
	n = sis(S_READ, fd, buf, 100);
	mira(n == 11 && iguales(buf, "sigo aqu\xc3\xad\n", 11), "pero el abierto se sigue leyendo");
	sis(S_CLOSE, fd, 0, 0);

	/* desde una carpeta abierta: openat, newfstatat, mkdirat, renameat, unlinkat */
	dfd = sis(S_OPEN, d, O_RDONLY | O_DIRECTORY, 0);
	mira(dfd >= 0, "abrir la carpeta (O_DIRECTORY)");
	fd = sis6(S_OPENAT, dfd, (i64)"desde.txt", O_CREAT | O_WRONLY, 0644, 0, 0);
	mira(fd >= 0 && sis(S_WRITE, fd, "openat\n", 7) == 7, "openat con O_CREAT");
	sis(S_CLOSE, fd, 0, 0);
	mira(sis6(S_NEWFSTATAT, dfd, (i64)"desde.txt", (i64)&e, 0, 0, 0) == 0 && e.tamaño == 7, "newfstatat");
	mira(sis6(S_NEWFSTATAT, dfd, (i64)"", (i64)&e, 0x1000, 0, 0) == 0 && (e.modo & 0170000) == 0040000, "newfstatat con AT_EMPTY_PATH");
	mira(sis(S_MKDIRAT, dfd, "hecha con mkdirat", 0755) == 0, "mkdirat");
	mira(sis6(S_RENAMEAT, dfd, (i64)"desde.txt", dfd, (i64)"hecha con mkdirat/renombrado.txt", 0, 0) == 0, "renameat");
	mira(sis6(S_OPENAT, dfd, (i64)"hecha con mkdirat/renombrado.txt", O_RDONLY, 0, 0, 0) >= 0, "openat de lo renombrado");
	mira(sis(S_UNLINKAT, dfd, "hecha con mkdirat", 0x200) == -ENOTEMPTY_, "rmdir de una no vac\xc3\xad""a: ENOTEMPTY");
	mira(sis(S_UNLINKAT, dfd, "hecha con mkdirat/renombrado.txt", 0) == 0, "unlinkat");
	mira(sis(S_UNLINKAT, dfd, "hecha con mkdirat", 0x200) == 0, "unlinkat con AT_REMOVEDIR");
	mira(sis6(S_OPENAT, 1, (i64)"x", O_RDONLY, 0, 0, 0) == -ENOTDIR_, "openat desde la consola: ENOTDIR");

	/* chdir y getcwd */
	mira(sis(S_CHDIR, d, 0, 0) == 0, "chdir");
	mira(sis(S_GETCWD, t, sizeof t, 0) > 0 && igual(t, d), "getcwd");
	mira(dice("adios.txt", "Hola, MUNDO\nsegunda l\xc3\xadnea\n"), "abrir sin ruta, desde la carpeta actual");
	mira(dice("./sub2/../adios.txt", "Hola, MUNDO\nsegunda l\xc3\xadnea\n"), "con \xc2\xab.\xc2\xbb y \xc2\xab..\xc2\xbb");
	mira(sis(S_CHDIR, "/", 0, 0) == 0, "chdir a /");

	/* los errores */
	junta(ruta, d, "no est\xc3\xa1.txt");
	mira(abre(ruta, O_RDONLY) == -ENOENT, "ENOENT");
	mira(abre(d, O_WRONLY) == -EISDIR_, "abrir una carpeta para escribir: EISDIR");
	junta(ruta, d, "adios.txt");
	mira(sis(S_OPEN, ruta, O_RDONLY | O_DIRECTORY, 0) == -ENOTDIR_, "O_DIRECTORY de un archivo: ENOTDIR");
	junta(ruta, d, "adios.txt/dentro");
	mira(abre(ruta, O_RDONLY) == -ENOTDIR_, "pasar por un archivo: ENOTDIR");
	junta(ruta, d, "sub2");
	mira(sis(S_UNLINK, ruta, 0, 0) == -EISDIR_, "unlink de una carpeta: EISDIR");
	mira(sis(S_RMDIR, ruta, 0, 0) == -ENOTEMPTY_, "rmdir de una no vac\xc3\xad""a: ENOTEMPTY");
	junta(ruta, d, "adios.txt");
	mira(sis(S_RMDIR, ruta, 0, 0) == -ENOTDIR_, "rmdir de un archivo: ENOTDIR");
	junta(ruta, d, "dos:puntos.txt");
	mira(abre(ruta, O_CREAT | O_WRONLY) == -EINVAL, "un nombre con \xc2\xab:\xc2\xbb: EINVAL");
	for (i = 0; i < 300; i++) largo_[i] = 'a';
	largo_[300] = 0;
	junta(ruta, d, largo_);
	mira(abre(ruta, O_CREAT | O_WRONLY) == -ENAMETOOLONG_, "un nombre de 300 letras: ENAMETOOLONG");
	/* lo más largo: 255 letras (las de UTF-16 de FAT y exFAT); 256, no */
	largo_[255] = 0;
	largo_[254] = 'Z';
	junta(ruta, d, largo_);
	mira(pon_archivo(ruta, "255\n") == 0 && dice(ruta, "255\n"), "un nombre de 255 letras");
	mira(cuenta(d, largo_, &está) > 0 && está, "y getdents64 lo da entero");
	mira(sis(S_UNLINK, ruta, 0, 0) == 0, "y se borra");
	largo_[255] = 'a';
	largo_[256] = 0;
	junta(ruta, d, largo_);
	mira(abre(ruta, O_CREAT | O_WRONLY) == -ENAMETOOLONG_, "uno de 256: ENAMETOOLONG");
	junta(ruta, d, "adios.txt");
	mira(sis(S_RENAME, ruta, "/HOLA", 0) == -EXDEV_, "rename a otro montaje: EXDEV");
	mira(abre("/NUEVO.TXT", O_CREAT | O_WRONLY) == -EROFS, "crear en la unidad en memoria: EROFS");
	mira(sis(S_RMDIR, m, 0, 0) == -EBUSY_, "rmdir de un montaje: EBUSY");
	/* dup: comparten la posici\xc3\xb3n */
	fd = abre(ruta, O_RDONLY);
	fd2 = sis(S_DUP, fd, 0, 0);
	sis(S_READ, fd, buf, 5);
	mira(fd2 > fd && sis(S_LSEEK, fd2, 0, 1) == 5, "dup: comparten la posici\xc3\xb3n");
	sis(S_CLOSE, fd, 0, 0);
	mira(sis(S_READ, fd2, buf, 1) == 1 && buf[0] == ' ', "y el otro sigue abierto");
	sis(S_CLOSE, fd2, 0, 0);
	/* el último, sin fsync: lo escribe el apagado (o el vaciador) */
	mira(sis(S_SYNC, 0, 0, 0) == 0, "sync");
	junta(ruta, m, "ULTIMO.TXT");
	mira(pon_archivo(ruta, "Escrito sin fsync: lo vac\xc3\xad""a el apagado.\n") == 0, "ULTIMO.TXT, sin fsync");
	pon("  la prueba en "); pon(d); pon(": crear, escribir, leer, O_APPEND, pwrite, 1,2 MB, huecos, ftruncate, ");
	pon("nombres largos, 40 archivos, rename, unlink abierto, *at, chdir, los errores\n");
}

/* la raíz fija de FAT12 y FAT16 (la marca RAIZFIJA.PRB): se llena (ENOSPC) y se vacía */
static void raíz_fija(const char *m)
{
	int k, hechos = 0, bien = 1, antes, está;
	i64 r = 0;
	char nom[16] = "RAIZ000.TXT";
	antes = cuenta(m, 0, 0);
	for (k = 0; k < 1000; k++) {
		nom[4] = (char)('0' + k / 100); nom[5] = (char)('0' + k / 10 % 10); nom[6] = (char)('0' + k % 10);
		junta(ruta, m, nom);
		r = abre(ruta, O_CREAT | O_WRONLY);
		if (r < 0) break;
		sis(S_CLOSE, r, 0, 0);
		hechos++;
	}
	mira(r == -ENOSPC_ && hechos > 0 && hechos < 1000, "la ra\xc3\xadz fija se llena: ENOSPC");
	mira(sis(S_MKDIR, ruta, 0755, 0) == -ENOSPC_, "mkdir en la ra\xc3\xadz llena: ENOSPC");
	mira(cuenta(m, "RAIZ000.TXT", &está) == antes + hechos && está, "getdents64 de la ra\xc3\xadz llena");
	for (k = 0; k < hechos; k++) {
		nom[4] = (char)('0' + k / 100); nom[5] = (char)('0' + k / 10 % 10); nom[6] = (char)('0' + k % 10);
		junta(ruta, m, nom);
		if (sis(S_UNLINK, ruta, 0, 0) != 0) bien = 0;
	}
	mira(bien && cuenta(m, 0, 0) == antes, "y se vac\xc3\xad""a otra vez");
	pon("  la ra\xc3\xadz fija: "); pon_n(hechos); pon(" archivos hasta ENOSPC, y borrados\n");
}

static void rápido(const char *m)
{
	i64 fd, n, r, i, t0, t1;
	u64 ts[2];
	int bien = 1;
	junta(ruta, m, "RAPIDO.BIN");
	for (i = 0; i < 65536; i++) buf[i] = (char)patrón((u64)i);
	sis(S_CLOCK_GETTIME, 1, ts, 0);
	t0 = (i64)(ts[0] * 1000000 + ts[1] / 1000);
	fd = abre(ruta, O_CREAT | O_WRONLY | O_TRUNC);
	for (n = 0; n < (8 << 20) && fd >= 0; n += 65536) if (sis(S_WRITE, fd, buf, 65536) != 65536) bien = 0;
	sis(S_FSYNC, fd, 0, 0);
	sis(S_CLOSE, fd, 0, 0);
	sis(S_CLOCK_GETTIME, 1, ts, 0);
	t1 = (i64)(ts[0] * 1000000 + ts[1] / 1000);
	pon("  escribir 8 MB (y fsync): "); pon_n((t1 - t0) / 1000); pon(" ms, ");
	pon_n((8LL * 1000000) / ((t1 - t0) > 0 ? (t1 - t0) : 1)); pon(" MB/s\n");
	t0 = t1;
	fd = abre(ruta, O_RDONLY);
	for (n = 0; (r = sis(S_READ, fd, buf2, 65536)) > 0; n += r) if (!iguales(buf, buf2, (u64)r)) bien = 0;
	sis(S_CLOSE, fd, 0, 0);
	sis(S_CLOCK_GETTIME, 1, ts, 0);
	t1 = (i64)(ts[0] * 1000000 + ts[1] / 1000);
	pon("  leer 8 MB: "); pon_n((t1 - t0) / 1000); pon(" ms, ");
	pon_n((8LL * 1000000) / ((t1 - t0) > 0 ? (t1 - t0) : 1)); pon(" MB/s\n");
	mira(bien && n == (8 << 20), "8 MB escritos y le\xc3\xad""dos");
	sis(S_UNLINK, ruta, 0, 0);
}

int principal(i64 *sp)
{
	i64 argc = sp[0], fd, n, k;
	char **argv = (char **)(sp + 1);
	static char montajes[16][64];
	int nm = 0, i, de_prueba = 0, escritos = 0;

	if (argc >= 3 && igual(argv[1], "dir")) {
		pon(argv[2]); pon(":\n");
		dir(argv[2], 0);
		return 0;
	}
	if (argc >= 3 && igual(argv[1], "rapido")) {
		rápido(argv[2]);
		return fallos;
	}
	/* auto: los montajes, de la lista de «/» */
	fd = sis(S_OPEN, "/", O_RDONLY | O_DIRECTORY, 0);
	mira(fd >= 0, "abrir /");
	while (fd >= 0 && (n = sis(S_GETDENTS64, fd, buf2, 4096)) > 0)
		for (k = 0; k < n; k += *(unsigned short *)(buf2 + k + 16))
			if (buf2[k + 18] == 4 && !igual(buf2 + k + 19, ".") && !igual(buf2 + k + 19, "..") && nm < 16)
				junta(montajes[nm++], "/", buf2 + k + 19);
	sis(S_CLOSE, fd, 0, 0);
	pon("ARCHIVOS: "); pon_n(nm); pon(" vol\xc3\xbamenes en /:");
	for (i = 0; i < nm; i++) { pon(" "); pon(montajes[i]); }
	pon("\n");
	for (i = 0; i < nm; i++) {
		junta(ruta, montajes[i], "HISPA64.PRB");
		if (tamaño_de(ruta) < 0) continue;
		de_prueba++;
		pon("ARCHIVOS en "); pon(montajes[i]); pon(" (un disco de prueba):\n");
		lee_anfitrión(montajes[i]);
		/* ¿se puede escribir? */
		junta(ruta, montajes[i], "escribir.prb");
		fd = abre(ruta, O_CREAT | O_WRONLY);
		if (fd == -EROFS) {
			pon("  de solo leer (EROFS): no se escribe\n");
			mira(sis(S_MKDIR, ruta, 0755, 0) == -EROFS, "mkdir en uno de solo leer: EROFS");
			continue;
		}
		mira(fd >= 0, "crear un archivo");
		sis(S_CLOSE, fd, 0, 0);
		mira(sis(S_UNLINK, ruta, 0, 0) == 0, "y borrarlo");
		prueba(montajes[i]);
		junta(ruta, montajes[i], "RAIZFIJA.PRB");
		if (tamaño_de(ruta) >= 0) raíz_fija(montajes[i]);
		escritos++;
	}
	pon("ARCHIVOS: "); pon_n(de_prueba); pon(de_prueba == 1 ? " disco de prueba (" : " discos de prueba (");
	pon_n(escritos); pon(escritos == 1 ? " escrito); " : " escritos); ");
	if (fallos == 0) { pon("todo bien ("); pon_n(pruebas); pon(" pruebas)\n"); }
	else { pon_n(fallos); pon(" MAL de "); pon_n(pruebas); pon("\n"); }
	return fallos;
}
