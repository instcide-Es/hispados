/* ordenes.c - Hispa-64, etapa 6 (docs/64/HISPA-64.md, sección 12): ORDENES,
 * el intérprete de órdenes en el anillo 3, en español. Un ELF64 estático
 * sin biblioteca (con sis.h), que corre igual en Linux.
 *
 * Lee una línea de la consola (la edita el núcleo: Tty64) con el indicador de
 * la carpeta actual («/sata0p1> ») y hace la orden:
 *   dir, ls [RUTA...]          lo que hay (con los tamaños y las carpetas)
 *   cd [RUTA]                  cambia de carpeta (sin nada, dice cuál es)
 *   pwd                        la carpeta actual
 *   type, cat ARCHIVO...       enseña lo que tienen
 *   mkdir, md CARPETA...       crea carpetas
 *   rmdir, rd CARPETA...       borra carpetas vacías
 *   rm, del, borra ARCHIVO...  borra archivos
 *   cp, copy DE A              copia un archivo (A puede ser una carpeta)
 *   mv, ren, move DE A         cambia el nombre o mueve (a otro volumen: copia y borra)
 *   echo TEXTO                 lo escribe
 *   cls                        borra la pantalla
 *   ver                        la versión
 *   ayuda                      esto
 *   salir, exit [CÓDIGO]       se acaba
 * y cualquier otra palabra es un programa: por su ruta («/sata0p1/boot/prog/HOLA»)
 * o por su nombre, buscado en la carpeta actual y en «/» (la unidad en
 * memoria), con sus argumentos (fork, execve y wait4); si acaba con un
 * código que no es 0, o lo cierra una señal, se dice. Con «> ARCHIVO» (o
 * «>> ARCHIVO», que añade) lo que se escribe va a un archivo, y con
 * «< ARCHIVO» se lee de él. Las comillas ("..." o '...') juntan palabras.
 * Los errores, en español, con el número de error de Linux explicado.
 * Ctrl+C cierra el programa que corre (el intérprete sigue).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "sis.h"

enum { S_STAT_ = 4, S_FSTAT_ = 5, S_DUP2_ = 33, S_GETCWD_ = 79, S_CHDIR_ = 80, S_RENAME_ = 82,
       S_MKDIR_ = 83, S_RMDIR_ = 84, S_UNLINK_ = 87, S_GETDENTS64_ = 217 };
enum { EINTR_ = 4, EXDEV_ = 18, EISDIR_ = 21, ENOTDIR_ = 20, ENOEXEC_ = 8, EACCES_ = 13 };

#define LARGO 1024
#define MAX_PAL 64

static char **entorno;
static int es_terminal;
static int salida = 1;                 /* adonde escriben las órdenes de dentro (> archivo) */
static char sb[4096];
static u64 sn;

/* ---- escribir ---- */
static void vacía(void)
{
	u64 h = 0;
	i64 r;
	while (h < sn) {
		r = sis(S_WRITE, salida, sb + h, sn - h);
		if (r <= 0) break;
		h += (u64)r;
	}
	sn = 0;
}
static void di(const char *s)
{
	while (*s) {
		if (sn == sizeof sb) vacía();
		sb[sn++] = *s++;
	}
}
static void di_trozo(const char *s, u64 n)
{
	while (n--) {
		if (sn == sizeof sb) vacía();
		sb[sn++] = *s++;
	}
}
static void di_n(u64 v)
{
	char t[24]; int i = 23;
	t[i] = 0;
	do { t[--i] = '0' + v % 10; v /= 10; } while (v);
	di(t + i);
}
/* con los miles separados («1.234.567»), a la derecha en «ancho» */
static void di_miles(u64 v, int ancho)
{
	char t[40]; int i = 39, k = 0;
	t[i] = 0;
	do {
		if (k && k % 3 == 0) t[--i] = '.';
		t[--i] = '0' + v % 10; v /= 10; k++;
	} while (v);
	while (39 - i < ancho) t[--i] = ' ';
	di(t + i);
}
static void al_error(const char *s) { sis(S_WRITE, 2, s, largo(s)); }

static int igual(const char *a, const char *b) { while (*a && *a == *b) { a++; b++; } return *a == *b; }
static void copia(char *d, const char *s, u64 máx) { u64 i = 0; while (s[i] && i + 1 < máx) { d[i] = s[i]; i++; } d[i] = 0; }
static void junta(char *d, const char *a, const char *b, u64 máx)
{
	u64 i = 0, j = 0;
	while (a[i] && i + 2 < máx) { d[i] = a[i]; i++; }
	if (i > 0 && d[i - 1] != '/' && i + 2 < máx) d[i++] = '/';
	while (b[j] && i + 1 < máx) d[i++] = b[j++];
	d[i] = 0;
}
static const char *último(const char *r)
{
	const char *u = r;
	while (*r) { if (*r == '/' && r[1]) u = r + 1; r++; }
	return u;
}

/* ---- los errores de Linux, explicados ---- */
static const char *explica(i64 e)
{
	switch (e) {
	case 1: return "no se permite";
	case 2: return "no existe";
	case 5: return "error del disco (de entrada y salida)";
	case 7: return "demasiados argumentos";
	case 8: return "no es un programa (no es un ELF64 que se sepa arrancar)";
	case 9: return "el descriptor no vale";
	case 10: return "no hay hijos que esperar";
	case 11: return "hay demasiados procesos (pruebe otra vez)";
	case 12: return "no queda memoria";
	case 13: return "no se puede (es una carpeta, o no hay permiso)";
	case 14: return "una direcci\xc3\xb3n que no vale";
	case 16: return "est\xc3\xa1 en uso (\xc2\xbf" "es un volumen montado?)";
	case 17: return "ya existe";
	case 18: return "est\xc3\xa1 en otro volumen";
	case 20: return "no es una carpeta";
	case 21: return "es una carpeta";
	case 22: return "no vale (\xc2\xbfun nombre con letras que no se pueden usar?)";
	case 24: return "hay demasiados archivos abiertos";
	case 25: return "no es una terminal";
	case 27: return "es demasiado grande";
	case 28: return "no queda sitio en el disco";
	case 30: return "es de solo leer (la unidad en memoria, o un disco sin \xc2\xab" "escribir\xc2\xbb)";
	case 36: return "el nombre es demasiado largo";
	case 38: return "Hispa-64 a\xc3\xban no sabe hacerlo";
	case 39: return "la carpeta no est\xc3\xa1 vac\xc3\xad" "a";
	default: return "error";
	}
}
static const char *nombre_error(i64 e)
{
	switch (e) {
	case 1: return "EPERM"; case 2: return "ENOENT"; case 5: return "EIO"; case 7: return "E2BIG";
	case 8: return "ENOEXEC"; case 9: return "EBADF"; case 10: return "ECHILD"; case 11: return "EAGAIN";
	case 12: return "ENOMEM"; case 13: return "EACCES"; case 14: return "EFAULT"; case 16: return "EBUSY";
	case 17: return "EEXIST"; case 18: return "EXDEV"; case 20: return "ENOTDIR"; case 21: return "EISDIR";
	case 22: return "EINVAL"; case 24: return "EMFILE"; case 25: return "ENOTTY"; case 27: return "EFBIG";
	case 28: return "ENOSPC"; case 30: return "EROFS"; case 36: return "ENAMETOOLONG"; case 38: return "ENOSYS";
	case 39: return "ENOTEMPTY"; default: return "?";
	}
}
/* «orden: «qué»: explicación (ENOENT, error 2 de Linux)» */
static void falla(const char *orden, const char *qué, i64 r)
{
	char t[600], n[24];
	u64 e = (u64)(-r), k = 0;
	int i = 23;
	n[i] = 0;
	do { n[--i] = '0' + e % 10; e /= 10; } while (e);
	t[0] = 0;
	junta(t, "", orden, sizeof t);
	k = largo(t);
	copia(t + k, ": \xc2\xab", sizeof t - k); k = largo(t);
	copia(t + k, qué, sizeof t - k - 100); k = largo(t);
	copia(t + k, "\xc2\xbb: ", sizeof t - k); k = largo(t);
	copia(t + k, explica(-r), sizeof t - k); k = largo(t);
	copia(t + k, " (", sizeof t - k); k = largo(t);
	copia(t + k, nombre_error(-r), sizeof t - k); k = largo(t);
	copia(t + k, ", error ", sizeof t - k); k = largo(t);
	copia(t + k, n + i, sizeof t - k); k = largo(t);
	copia(t + k, " de Linux)\n", sizeof t - k);
	vacía();
	al_error(t);
}

/* ---- lo de los archivos ---- */
struct estado { u64 dev, ino, nlink; unsigned modo, uid, gid, pad; u64 rdev; i64 tam, blksize, bloques; u64 resto[9]; };   /* los 144 octetos de Linux */
static int es_carpeta(const char *r)
{
	struct estado st;
	return sis(S_STAT_, r, &st, 0) == 0 && (st.modo & 0xF000) == 0x4000;
}
static int es_archivo(const char *r)
{
	struct estado st;
	return sis(S_STAT_, r, &st, 0) == 0 && (st.modo & 0xF000) == 0x8000;
}

static void lista(const char *ruta, int varias)
{
	char buf[4096], hija[LARGO];
	struct estado st;
	i64 fd, n, i;
	u64 archivos = 0, carpetas = 0, total = 0;
	if (sis(S_STAT_, ruta, &st, 0) < 0) {
		falla("dir", ruta, sis(S_STAT_, ruta, &st, 0));
		return;
	}
	if ((st.modo & 0xF000) != 0x4000) {
		di_miles((u64)st.tam, 14); di("  "); di(ruta); di("\n");
		return;
	}
	fd = sis(S_OPEN, ruta, 0x10000, 0);           /* O_DIRECTORY */
	if (fd < 0) { falla("dir", ruta, fd); return; }
	if (varias) { di(ruta); di(":\n"); }
	while ((n = sis(S_GETDENTS64_, fd, buf, sizeof buf)) > 0) {
		for (i = 0; i < n; ) {
			unsigned short rec = *(unsigned short *)(buf + i + 16);
			char *nombre = buf + i + 19;
			if (!igual(nombre, ".") && !igual(nombre, "..")) {
				junta(hija, ruta, nombre, sizeof hija);
				if (sis(S_STAT_, hija, &st, 0) == 0 && (st.modo & 0xF000) == 0x4000) {
					di("         <DIR>  "); di(nombre); di("/\n");
					carpetas++;
				} else {
					if (sis(S_STAT_, hija, &st, 0) != 0) st.tam = 0;
					di_miles((u64)st.tam, 14); di("  "); di(nombre); di("\n");
					archivos++;
					total += (u64)st.tam;
				}
			}
			i += rec;
			if (rec == 0) break;
		}
	}
	if (n < 0) falla("dir", ruta, n);
	sis(S_CLOSE, fd, 0, 0);
	di("  "); di_n(archivos); di(archivos == 1 ? " archivo (" : " archivos ("); di_miles(total, 0);
	di(total == 1 ? " octeto); " : " octetos); "); di_n(carpetas); di(carpetas == 1 ? " carpeta\n" : " carpetas\n");
}

static void enseña(const char *r)
{
	char buf[8192];
	i64 fd, n;
	fd = sis(S_OPEN, r, 0, 0);
	if (fd < 0) { falla("type", r, fd); return; }
	while ((n = sis(S_READ, fd, buf, sizeof buf)) > 0) di_trozo(buf, (u64)n);
	if (n < 0) falla("type", r, n);
	sis(S_CLOSE, fd, 0, 0);
}

/* copia de a a (si a es una carpeta, dentro con el mismo nombre); 0 o el error */
static i64 copia_archivo(const char *orden, const char *de, const char *a)
{
	char buf[16384], dest[LARGO];
	i64 fi, fo, n, m, h;
	if (es_carpeta(a)) junta(dest, a, último(de), sizeof dest); else copia(dest, a, sizeof dest);
	if (es_carpeta(de)) { falla(orden, de, -EISDIR_); return -EISDIR_; }
	fi = sis(S_OPEN, de, 0, 0);
	if (fi < 0) { falla(orden, de, fi); return fi; }
	fo = sis(S_OPEN, dest, 0x241, 0644);          /* O_WRONLY | O_CREAT | O_TRUNC */
	if (fo < 0) { falla(orden, dest, fo); sis(S_CLOSE, fi, 0, 0); return fo; }
	while ((n = sis(S_READ, fi, buf, sizeof buf)) > 0) {
		for (h = 0; h < n; h += m) {
			m = sis(S_WRITE, fo, buf + h, n - h);
			if (m <= 0) { falla(orden, dest, m < 0 ? m : -28); n = -1; break; }
		}
		if (n < 0) break;
	}
	if (n < 0) { sis(S_CLOSE, fi, 0, 0); sis(S_CLOSE, fo, 0, 0); return -5; }
	sis(S_CLOSE, fi, 0, 0);
	sis(S_CLOSE, fo, 0, 0);
	return 0;
}

static void mueve(const char *de, const char *a)
{
	char dest[LARGO];
	i64 r;
	if (es_carpeta(a)) junta(dest, a, último(de), sizeof dest); else copia(dest, a, sizeof dest);
	r = sis(S_RENAME_, de, dest, 0);
	if (r == -EXDEV_ && es_archivo(de)) {
		/* a otro volumen: se copia y se borra */
		if (copia_archivo("mv", de, dest) == 0) {
			r = sis(S_UNLINK_, de, 0, 0);
			if (r < 0) falla("mv", de, r);
		}
		return;
	}
	if (r < 0) falla("mv", de, r);
}

/* ---- arrancar un programa ---- */
static const char *nombre_señal(int s)
{
	switch (s) {
	case 2: return "SIGINT: Ctrl+C";
	case 4: return "SIGILL: una instrucci\xc3\xb3n que no existe";
	case 6: return "SIGABRT: se ha abortado";
	case 7: return "SIGBUS: un dato desalineado";
	case 8: return "SIGFPE: una cuenta imposible";
	case 9: return "SIGKILL: cerrado a la fuerza";
	case 11: return "SIGSEGV: una direcci\xc3\xb3n que no es suya";
	case 13: return "SIGPIPE";
	case 15: return "SIGTERM";
	default: return "una se\xc3\xb1" "al";
	}
}

/* la ruta del programa «n» (si no lleva «/»: en la carpeta actual y en «/»); 0 si no está */
static int busca(const char *n, char *r)
{
	char cwd[LARGO];
	int i;
	for (i = 0; n[i]; i++) if (n[i] == '/') { copia(r, n, LARGO); return es_archivo(r) || es_carpeta(r); }
	if (sis(S_GETCWD_, cwd, sizeof cwd, 0) > 0) {
		junta(r, cwd, n, LARGO);
		if (es_archivo(r)) return 1;
	}
	junta(r, "/", n, LARGO);
	return es_archivo(r);
}

static void redirige_en_el_hijo(const char *a, int añade, const char *de)
{
	i64 fd;
	if (a) {
		fd = sis(S_OPEN, a, añade ? 0x441 : 0x241, 0644);
		if (fd < 0) { falla("ORDENES", a, fd); sis(S_EXIT_GROUP, 1, 0, 0); }
		sis(S_DUP2_, fd, 1, 0);
		sis(S_CLOSE, fd, 0, 0);
	}
	if (de) {
		fd = sis(S_OPEN, de, 0, 0);
		if (fd < 0) { falla("ORDENES", de, fd); sis(S_EXIT_GROUP, 1, 0, 0); }
		sis(S_DUP2_, fd, 0, 0);
		sis(S_CLOSE, fd, 0, 0);
	}
}

static void arranca(int n, char **pal, const char *a, int añade, const char *de)
{
	char ruta[LARGO];
	i64 pid, r;
	int st = 0;
	if (!busca(pal[0], ruta)) {
		vacía();
		al_error("No hay ninguna orden ni ning\xc3\xban programa que se llame \xc2\xab");
		al_error(pal[0]);
		al_error("\xc2\xbb. Escriba \xc2\xab" "ayuda\xc2\xbb para ver las \xc3\xb3rdenes.\n");
		return;
	}
	if (es_carpeta(ruta)) { falla(pal[0], ruta, -EISDIR_); return; }
	(void)n;
	vacía();
	pid = sis(S_FORK, 0, 0, 0);
	if (pid < 0) { falla(pal[0], "fork", pid); return; }
	if (pid == 0) {
		redirige_en_el_hijo(a, añade, de);
		r = sis(S_EXECVE, ruta, pal, entorno);
		falla(pal[0], ruta, r);
		sis(S_EXIT_GROUP, 127, 0, 0);
	}
	do r = sis6(S_WAIT4, pid, (i64)&st, 0, 0, 0, 0); while (r == -EINTR_);
	if (r < 0) { falla(pal[0], "wait4", r); return; }
	if ((st & 0x7F) != 0) {
		di("("); di(último(ruta)); di(" se ha cerrado por la se\xc3\xb1" "al "); di_n((u64)(st & 0x7F)); di(", ");
		di(nombre_señal(st & 0x7F)); di(")\n");
	} else if ((st >> 8) & 0xFF) {
		di("("); di(último(ruta)); di(" ha acabado con el c\xc3\xb3" "digo "); di_n((u64)((st >> 8) & 0xFF)); di(")\n");
	}
}

/* ---- las órdenes ---- */
static void ayuda(void)
{
	di("Las \xc3\xb3rdenes de ORDENES:\n"
	   "  dir, ls [RUTA...]          lo que hay en una carpeta (con los tama\xc3\xb1os)\n"
	   "  cd [RUTA]                  cambia de carpeta (sin nada: dice en cu\xc3\xa1l est\xc3\xa1)\n"
	   "  pwd                        la carpeta actual\n"
	   "  type, cat ARCHIVO...       ense\xc3\xb1" "a lo que tienen\n"
	   "  mkdir, md CARPETA...       crea carpetas\n"
	   "  rmdir, rd CARPETA...       borra carpetas vac\xc3\xad" "as\n"
	   "  rm, del, borra ARCHIVO...  borra archivos\n"
	   "  cp, copy DE A              copia un archivo (A puede ser una carpeta)\n"
	   "  mv, ren, move DE A         cambia el nombre o mueve\n"
	   "  echo TEXTO                 lo escribe\n"
	   "  cls                        borra la pantalla\n"
	   "  ver                        la versi\xc3\xb3n\n"
	   "  ayuda                      esto\n"
	   "  salir, exit [C\xc3\x93" "DIGO]       se acaba (en Hispa-64, se apaga el equipo)\n"
	   "Cualquier otra palabra es un programa (por su ruta, o por su nombre: en la\n"
	   "carpeta actual y en \xc2\xab/\xc2\xbb), con sus argumentos. \xc2\xab> ARCHIVO\xc2\xbb y \xc2\xab>> ARCHIVO\xc2\xbb\n"
	   "llevan lo que se escribe a un archivo; \xc2\xab< ARCHIVO\xc2\xbb, lo que se lee. Ctrl+C\n"
	   "cierra el programa que corre; con las flechas se edita la l\xc3\xad" "nea y se\n"
	   "vuelve a las de antes.\n");
}

static void ver(void)
{
	char u[390];
	di("ORDENES 1.0, el int\xc3\xa9rprete de \xc3\xb3rdenes de Hispa-64");
	if (sis(S_UNAME, u, 0, 0) == 0) {
		di(" (en "); di(u); di(" "); di(u + 130); di(", "); di(u + 195); di(", "); di(u + 260); di(")");
	}
	di("\nCopyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.\n");
}

static int es(const char *p, const char *a, const char *b, const char *c)
{
	return igual(p, a) || (b && igual(p, b)) || (c && igual(p, c));
}

/* parte la línea en palabras (con las comillas) y saca las redirecciones */
static int parte(char *l, char **pal, char **a, int *añade, char **de)
{
	static char t[2 * LARGO + 8];
	int n = 0, k = 0, espera = 0;       /* espera: 1 «>», 2 «>>», 3 «<» */
	char *ini, q;
	*a = 0; *de = 0; *añade = 0;
	while (*l) {
		while (*l == ' ' || *l == '\t' || *l == '\n' || *l == '\r') l++;
		if (!*l) break;
		if (*l == '>' || *l == '<') {
			if (espera) return -1;
			if (*l == '<') espera = 3; else if (l[1] == '>') { espera = 2; l++; } else espera = 1;
			l++;
			continue;
		}
		ini = t + k;
		while (*l && *l != ' ' && *l != '\t' && *l != '\n' && *l != '\r' && *l != '>' && *l != '<') {
			if (*l == '"' || *l == '\'') {
				q = *l++;
				while (*l && *l != q) t[k++] = *l++;
				if (*l) l++;
			} else {
				t[k++] = *l++;
			}
		}
		t[k++] = 0;
		if (espera == 1 || espera == 2) { *a = ini; *añade = espera == 2; espera = 0; continue; }
		if (espera == 3) { *de = ini; espera = 0; continue; }
		if (n < MAX_PAL - 1) pal[n++] = ini;
	}
	pal[n] = 0;
	if (espera) return -1;
	return n;
}

static void haz(char *l)
{
	char *pal[MAX_PAL], *a, *de;
	char t[LARGO];
	int n, i, añade;
	i64 r, fd = -1;
	n = parte(l, pal, &a, &añade, &de);
	if (n < 0) { vacía(); al_error("Falta el nombre del archivo despu\xc3\xa9s de \xc2\xab>\xc2\xbb o de \xc2\xab<\xc2\xbb.\n"); return; }
	if (n == 0) return;
	/* las de dentro, con «> archivo» */
	if (a && (es(pal[0], "dir", "ls", 0) || es(pal[0], "type", "cat", 0) || es(pal[0], "echo", "pwd", "ver") || es(pal[0], "ayuda", "cd", 0))) {
		fd = sis(S_OPEN, a, añade ? 0x441 : 0x241, 0644);
		if (fd < 0) { falla(pal[0], a, fd); return; }
		vacía();
		salida = (int)fd;
	}
	if (es(pal[0], "dir", "ls", 0)) {
		if (n == 1) lista(".", 0);
		for (i = 1; i < n; i++) lista(pal[i], n > 2);
	} else if (es(pal[0], "cd", "chdir", 0)) {
		if (n == 1) { if (sis(S_GETCWD_, t, sizeof t, 0) > 0) { di(t); di("\n"); } }
		else if ((r = sis(S_CHDIR_, pal[1], 0, 0)) < 0) falla("cd", pal[1], r);
	} else if (es(pal[0], "pwd", 0, 0)) {
		if (sis(S_GETCWD_, t, sizeof t, 0) > 0) { di(t); di("\n"); }
	} else if (es(pal[0], "type", "cat", 0)) {
		if (n == 1) { vacía(); al_error("type: falta el archivo\n"); }
		for (i = 1; i < n; i++) enseña(pal[i]);
	} else if (es(pal[0], "mkdir", "md", 0)) {
		if (n == 1) { vacía(); al_error("mkdir: falta la carpeta\n"); }
		for (i = 1; i < n; i++) if ((r = sis(S_MKDIR_, pal[i], 0755, 0)) < 0) falla("mkdir", pal[i], r);
	} else if (es(pal[0], "rmdir", "rd", 0)) {
		if (n == 1) { vacía(); al_error("rmdir: falta la carpeta\n"); }
		for (i = 1; i < n; i++) if ((r = sis(S_RMDIR_, pal[i], 0, 0)) < 0) falla("rmdir", pal[i], r);
	} else if (es(pal[0], "rm", "del", "borra")) {
		if (n == 1) { vacía(); al_error("rm: falta el archivo\n"); }
		for (i = 1; i < n; i++) {
			if (es_carpeta(pal[i])) { vacía(); al_error("rm: \xc2\xab"); al_error(pal[i]); al_error("\xc2\xbb es una carpeta: se borra con rmdir (o rd)\n"); continue; }
			if ((r = sis(S_UNLINK_, pal[i], 0, 0)) < 0) falla("rm", pal[i], r);
		}
	} else if (es(pal[0], "cp", "copy", 0)) {
		if (n != 3) { vacía(); al_error("cp: se usa as\xc3\xad: cp DE A\n"); }
		else copia_archivo("cp", pal[1], pal[2]);
	} else if (es(pal[0], "mv", "ren", "move")) {
		if (n != 3) { vacía(); al_error("mv: se usa as\xc3\xad: mv DE A\n"); }
		else mueve(pal[1], pal[2]);
	} else if (es(pal[0], "echo", 0, 0)) {
		for (i = 1; i < n; i++) { if (i > 1) di(" "); di(pal[i]); }
		di("\n");
	} else if (es(pal[0], "cls", "clear", 0)) {
		di("\x1b[2J\x1b[H");
	} else if (es(pal[0], "ver", 0, 0)) {
		ver();
	} else if (es(pal[0], "ayuda", "help", "?")) {
		ayuda();
	} else if (es(pal[0], "salir", "exit", 0)) {
		r = 0;
		if (n > 1) for (i = 0; pal[1][i] >= '0' && pal[1][i] <= '9'; i++) r = r * 10 + pal[1][i] - '0';
		vacía();
		sis(S_EXIT_GROUP, r, 0, 0);
	} else {
		arranca(n, pal, a, añade, de);
	}
	vacía();
	if (fd >= 0) { sis(S_CLOSE, fd, 0, 0); salida = 1; }
}

/* Ctrl+C: el intérprete no se cierra (su lectura se corta: -EINTR) */
static void al_ctrl_c(int s) { (void)s; }
__asm__(".text\nvuelve_de_senal:\n\tmov $15, %eax\n\tsyscall\n\tud2\n");
extern char vuelve_de_senal[];

/* una línea de la entrada (de la consola llega de una en una; de un tubo, quizá varias juntas) */
static char lb[LARGO];
static u64 lb_n;
static i64 lee_línea(char *l, u64 *n)
{
	u64 i;
	i64 r;
	for (;;) {
		for (i = 0; i < lb_n; i++) if (lb[i] == '\n') break;
		if (i < lb_n || lb_n == sizeof lb - 1) {
			if (i == lb_n) i--;
			memcpy(l, lb, i + 1);
			*n = i + 1;
			memcpy(lb, lb + i + 1, lb_n - i - 1);
			lb_n -= i + 1;
			return (i64)*n;
		}
		r = sis(S_READ, 0, lb + lb_n, sizeof lb - 1 - lb_n);
		if (r == -EINTR_) { lb_n = 0; return r; }
		if (r <= 0) {
			/* (lo que quede sin «\n», como una línea) */
			memcpy(l, lb, lb_n);
			*n = lb_n;
			lb_n = 0;
			return (i64)*n;
		}
		lb_n += (u64)r;
	}
}

int principal(i64 *sp)
{
	i64 argc = sp[0], r;
	char **argv = (char **)(sp + 1);
	char l[LARGO], cwd[LARGO];
	unsigned char tio[60];
	u64 act[4];
	u64 n;
	entorno = argv + argc + 1;
	es_terminal = sis(S_IOCTL, 0, TCGETS, tio) == 0;
	act[0] = (u64)al_ctrl_c; act[1] = 0x04000000; act[2] = (u64)vuelve_de_senal; act[3] = 0;     /* SA_RESTORER */
	sis6(S_SIGACTION, 2, (i64)act, 0, 8, 0, 0);
	di("ORDENES, el int\xc3\xa9rprete de \xc3\xb3rdenes de Hispa-64. Escriba \xc2\xab" "ayuda\xc2\xbb para ver las \xc3\xb3rdenes.\n");
	for (;;) {
		if (sis(S_GETCWD_, cwd, sizeof cwd, 0) <= 0) copia(cwd, "?", sizeof cwd);
		di(cwd); di("> ");
		vacía();
		r = lee_línea(l, &n);
		if (r == -EINTR_) { if (!es_terminal) di("\n"); continue; }
		l[n] = 0;
		if (r == 0 && n == 0) {
			di("salir\n");
			vacía();
			return 0;
		}
		if (!es_terminal) { di(l); if (n == 0 || l[n - 1] != '\n') di("\n"); }
		haz(l);
	}
}
