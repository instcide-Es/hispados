/* Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 *
 * corre32.c - corre en Linux un programa de usuario de Hispa-Pentium (un .BIN
 * de «modelo usuario», CASTEL32.md §8), para probar deprisa sin arrancar QEMU.
 * No sustituye a las pruebas dentro de Hispa-Pentium: sirve para no programar
 * a ciegas (y para comparar mucho y rápido, como hace pruebas.sh con CASTEL32).
 *
 *     corre32 [-C carpeta] PROGRAMA.BIN [argumentos...]
 *
 * Cómo: arranca un trocito de 32 bits (arranque.asm) que deja la memoria como
 * la pone Hispa-Pentium (el programa desde 0x40000000; la pila, hasta
 * 0xC0000000) y se para; aquí se le pone el programa y se le sigue con ptrace:
 * cada «int 0x80» (que en Linux sería una llamada de Linux) se para antes de
 * hacerse, se anula y se hace aquí, con los números de nucleo32/usuario.cas
 * (escribir, salir, los archivos, los argumentos, la memoria que crece...).
 * Las rutas son las de Hispa-Pentium («\» o «/», con letra de unidad o sin
 * ella: todas las unidades son la carpeta -C, o la actual) y, como en la FAT,
 * no distinguen mayúsculas de minúsculas.
 * Lo que sale por la llamada 1 (escribir) va a la salida de Linux; el código
 * de salida del programa es el de corre32. Con CORRE32_TRAZA=1, dice por la
 * salida de errores cada llamada.
 */
#define _GNU_SOURCE
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/stat.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define NO_HAY      0xFFFFFFFFu
#define MAL         0xFFFFFFFCu
#define SIN_MEMORIA 0xFFFFFFFAu
#define NO_VALE     0xFFFFFFF9u
#define YA_EXISTE   0xFFFFFFF7u
#define NO_VACIA    0xFFFFFFF4u
#define SIN_SALIDA  0xFFFFFFFFu
#define BASE        0x40000000u
#define TOPE        0xA0000000u           /* lo que deja el arranque para el programa */
#define MAX_ABIERTOS 64
#define PRIMER_MANEJADOR 16

static pid_t hijo;
static int mem;                               /* /proc/hijo/mem */
static int traza;
static char raiz[4096] = ".";
static char args_prog[512];
static uint32_t quiebre, quiebre_inicio;
static int abiertos[MAX_ABIERTOS];            /* el descriptor de Linux, o -1 */
static int salida = 1;                        /* adonde va escribir (redirigir) */
static char carpeta[512] = "";                /* la de trabajo, desde la raíz, con «/» */

static void muere(const char *m) { fprintf(stderr, "corre32: %s\n", m); if (hijo > 0) kill(hijo, SIGKILL); exit(120); }

static int lee_mem(uint32_t dir, void *p, size_t n) { return pread(mem, p, n, dir) == (ssize_t)n; }
static int pon_mem(uint32_t dir, const void *p, size_t n) { return pwrite(mem, p, n, dir) == (ssize_t)n; }

static int texto_de(uint32_t dir, char *t, size_t cabe)
{
    size_t i;
    for (i = 0; i + 1 < cabe; i++) {
        if (!lee_mem(dir + i, t + i, 1)) return 0;
        if (t[i] == 0) return 1;
    }
    t[i] = 0;
    return 1;
}

/* la ruta de Linux de una ruta de Hispa-Pentium; sin distinguir mayúsculas:
 * cada trozo se busca en su carpeta. «nueva»: el último trozo puede no estar */
static int ruta_linux(const char *r, char *sal, size_t cabe)
{
    char t[1024], *p, *trozo;
    size_t i;
    if (isalpha((unsigned char)r[0]) && r[1] == ':') r += 2;
    if (strlen(r) >= sizeof t) return 0;
    for (i = 0; r[i]; i++) t[i] = r[i] == '\\' ? '/' : r[i];
    t[i] = 0;
    if (t[0] == '/') snprintf(sal, cabe, "%s", raiz);
    else snprintf(sal, cabe, "%s%s%s", raiz, carpeta[0] ? "/" : "", carpeta);
    for (p = t; (trozo = strsep(&p, "/")) != NULL; ) {
        DIR *d;
        struct dirent *e;
        char hallado[512];
        if (!trozo[0] || !strcmp(trozo, ".")) continue;
        if (!strcmp(trozo, "..")) {
            char *b = strrchr(sal, '/');
            if (b && strlen(sal) > strlen(raiz)) *b = 0;
            continue;
        }
        snprintf(hallado, sizeof hallado, "%s", trozo);
        d = opendir(sal);
        if (d) {
            while ((e = readdir(d)) != NULL)
                if (!strcasecmp(e->d_name, trozo)) { snprintf(hallado, sizeof hallado, "%s", e->d_name); break; }
            closedir(d);
        }
        if (strlen(sal) + strlen(hallado) + 2 > cabe) return 0;
        strcat(sal, "/");
        strcat(sal, hallado);
    }
    return 1;
}

static uint32_t error_de(int e)
{
    if (e == ENOENT || e == ENOTDIR) return NO_HAY;
    if (e == EEXIST) return YA_EXISTE;
    if (e == ENOTEMPTY) return NO_VACIA;
    return MAL;
}

static int manejador(uint32_t m)
{
    if (m < PRIMER_MANEJADOR || m >= PRIMER_MANEJADOR + MAX_ABIERTOS) return -1;
    return abiertos[m - PRIMER_MANEJADOR];
}

static uint32_t abrir(uint32_t nombre, uint32_t modo)
{
    char n[512], r[4096];
    int fl, fd, i;
    if (!texto_de(nombre, n, sizeof n) || !ruta_linux(n, r, sizeof r)) return NO_VALE;
    switch (modo) {
        case 0: fl = O_RDONLY; break;
        case 1: fl = O_RDWR; break;
        case 2: fl = O_RDWR | O_CREAT | O_TRUNC; break;
        case 3: fl = O_RDWR | O_CREAT | O_APPEND; break;
        default: return MAL;
    }
    fd = open(r, fl, 0644);
    if (traza) fprintf(stderr, "[abrir «%s» -> «%s»: %d]\n", n, r, fd);
    if (fd < 0) return error_de(errno);
    for (i = 0; i < MAX_ABIERTOS; i++)
        if (abiertos[i] < 0) { abiertos[i] = fd; return PRIMER_MANEJADOR + i; }
    close(fd);
    return 0xFFFFFFFDu;
}

static uint32_t leer_o_escribir(int escribir, uint32_t m, uint32_t dir, uint32_t n)
{
    static char trozo[1 << 16];
    uint32_t hecho = 0;
    int fd = manejador(m);
    if (fd < 0) return MAL;
    while (hecho < n) {
        uint32_t k = n - hecho > sizeof trozo ? sizeof trozo : n - hecho;
        ssize_t r;
        if (escribir) {
            if (!lee_mem(dir + hecho, trozo, k)) return NO_VALE;
            r = write(fd, trozo, k);
        } else {
            r = read(fd, trozo, k);
            if (r > 0 && !pon_mem(dir + hecho, trozo, r)) return NO_VALE;
        }
        if (r < 0) return MAL;
        hecho += r;
        if ((uint32_t)r < k) break;
    }
    return hecho;
}

static uint32_t escribe(uint32_t dir, uint32_t n)
{
    static char trozo[1 << 16];
    uint32_t hecho = 0;
    while (hecho < n) {
        uint32_t k = n - hecho > sizeof trozo ? sizeof trozo : n - hecho;
        if (!lee_mem(dir + hecho, trozo, k)) return 0xFFFFFFFF;
        if (salida == 1) { if (fwrite(trozo, 1, k, stdout) != k) return MAL; }
        else if (write(salida, trozo, k) != (ssize_t)k) return MAL;
        hecho += k;
    }
    return n;
}

/* una llamada al sistema de Hispa-Pentium: n en EAX, a, b, c en EBX, ECX, EDX */
static uint32_t atender(uint32_t n, uint32_t a, uint32_t b, uint32_t c, int *acaba)
{
    char t[512], r[4096], r2[4096];
    int fd;
    struct stat st;
    switch (n) {
        case 1: return escribe(a, b);
        case 2: fflush(stdout); *acaba = 1; return a;
        case 3: return 0;
        case 4: { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return (uint32_t)(ts.tv_sec * 100 + ts.tv_nsec / 10000000); }
        case 5: return 1;
        case 6: return 0;
        case 7: return abrir(a, b);
        case 8: return leer_o_escribir(0, a, b, c);
        case 9:
            fd = manejador(a);
            if (fd >= 0) { close(fd); abiertos[a - PRIMER_MANEJADOR] = -1; if (salida == fd) salida = 1; }
            return 0;
        case 13:
            fd = manejador(a);
            if (fd < 0 || fstat(fd, &st) < 0) return MAL;
            return (uint32_t)st.st_size;
        case 14: {
            uint32_t largo = strlen(args_prog);
            if (b == 0) return NO_VALE;
            if (largo >= b) largo = b - 1;
            if (!pon_mem(a, args_prog, largo) || !pon_mem(a + largo, "", 1)) return NO_VALE;
            return largo;
        }
        case 15:
            switch (a) {
                case 0: return 16384;
                case 1: return 32768;
                case 3: return 0x0008;
                case 4: return 1;
                case 9: return 100;
                default: return 0;
            }
        case 17: {
            uint32_t antes = quiebre, nuevo = quiebre + a;
            if (a == 0) return antes;
            if (a < 0x80000000u) {
                if (nuevo < antes || nuevo > TOPE) return SIN_MEMORIA;
            } else {
                if (nuevo > antes || nuevo < quiebre_inicio) return NO_VALE;
            }
            quiebre = nuevo;
            return antes;
        }
        case 18: return leer_o_escribir(1, a, b, c);
        case 19: {
            off_t p;
            fd = manejador(a);
            if (fd < 0) return MAL;
            p = lseek(fd, (int32_t)b, c == 0 ? SEEK_SET : c == 1 ? SEEK_CUR : SEEK_END);
            return p < 0 ? MAL : (uint32_t)p;
        }
        case 20:
            fd = manejador(a);
            if (fd < 0) return MAL;
            return ftruncate(fd, lseek(fd, 0, SEEK_CUR)) < 0 ? MAL : 0;
        case 21: case 23: case 24: case 25:
            if (!texto_de(a, t, sizeof t) || !ruta_linux(t, r, sizeof r)) return NO_VALE;
            if (n == 21) return unlink(r) < 0 ? error_de(errno) : 0;
            if (n == 23) return mkdir(r, 0755) < 0 ? error_de(errno) : 0;
            if (n == 24) return rmdir(r) < 0 ? error_de(errno) : 0;
            if (stat(r, &st) < 0 || !S_ISDIR(st.st_mode)) return NO_HAY;
            if (strlen(r) <= strlen(raiz)) carpeta[0] = 0;
            else snprintf(carpeta, sizeof carpeta, "%s", r + strlen(raiz) + 1);
            return 0;
        case 22:
            if (!texto_de(a, t, sizeof t) || !ruta_linux(t, r, sizeof r)) return NO_VALE;
            if (!texto_de(b, t, sizeof t) || !ruta_linux(t, r2, sizeof r2)) return NO_VALE;
            return rename(r, r2) < 0 ? error_de(errno) : 0;
        case 26: {
            size_t i, k = 0;
            if (b == 0) return NO_VALE;
            t[k++] = '\\';
            for (i = 0; carpeta[i] && k + 1 < sizeof t; i++) t[k++] = carpeta[i] == '/' ? '\\' : toupper((unsigned char)carpeta[i]);
            if (k > 1 && carpeta[0] == 0) k = 1;
            if (k >= b) k = b - 1;
            t[k] = 0;
            return pon_mem(a, t, k + 1) ? k : NO_VALE;
        }
        case 27:
            fflush(stdout);
            if (a == SIN_SALIDA) { salida = 1; return 0; }
            fd = manejador(a);
            if (fd < 0) return MAL;
            salida = fd;
            return 0;
        case 29: return 0;
        case 42: return 'C';
    }
    fprintf(stderr, "corre32: la llamada %u no está hecha aquí\n", n);
    return NO_VALE;
}

int main(int argc, char **argv)
{
    int i, st, preparado = 0;
    const char *bin, *arranque;
    FILE *f;
    static unsigned char prog[0x200000 + 16];
    size_t largo;
    uint32_t magia, ini, datos_fin, vacio_fin, entrada;
    char ruta[64], exe[4096];
    struct user_regs_struct rg;

    i = 1;
    if (i + 1 < argc && !strcmp(argv[i], "-C")) { snprintf(raiz, sizeof raiz, "%s", argv[i + 1]); i += 2; }
    if (i >= argc) { fprintf(stderr, "uso: corre32 [-C carpeta] PROGRAMA.BIN [argumentos...]\n"); return 121; }
    bin = argv[i++];
    for (; i < argc; i++) {
        if (strlen(args_prog) + strlen(argv[i]) + 2 >= sizeof args_prog) muere("demasiados argumentos");
        if (args_prog[0]) strcat(args_prog, " ");
        strcat(args_prog, argv[i]);
    }
    /* la raíz, entera; y la carpeta de trabajo, la de Linux si está dentro */
    {
        char r[4096], cwd[4096];
        if (!realpath(raiz, r)) muere("la carpeta -C no existe");
        snprintf(raiz, sizeof raiz, "%s", strcmp(r, "/") ? r : "");
        if (getcwd(cwd, sizeof cwd) && !strncmp(cwd, raiz, strlen(raiz)) && (cwd[strlen(raiz)] == '/' || !cwd[strlen(raiz)]))
            snprintf(carpeta, sizeof carpeta, "%s", cwd[strlen(raiz)] ? cwd + strlen(raiz) + 1 : "");
    }
    for (i = 0; i < MAX_ABIERTOS; i++) abiertos[i] = -1;
    traza = getenv("CORRE32_TRAZA") != NULL;
    f = fopen(bin, "rb");
    if (!f) muere("no puedo abrir el programa");
    largo = fread(prog, 1, sizeof prog, f);
    fclose(f);
    if (largo < 28 || largo > 0x200000) muere("eso no es un programa de Hispa-Pentium (el tamaño)");
    for (i = 0; i <= 8; i += 4) {
        memcpy(&magia, prog + i, 4);
        if (magia == 0x33555048) break;
    }
    if (i > 8) muere("eso no es un programa de Hispa-Pentium (no lleva la cabecera HPU3)");
    memcpy(&ini, prog + i + 4, 4);
    memcpy(&datos_fin, prog + i + 8, 4);
    memcpy(&vacio_fin, prog + i + 12, 4);
    memcpy(&entrada, prog + i + 16, 4);
    if (ini != BASE || datos_fin < ini || vacio_fin < datos_fin || vacio_fin >= TOPE) muere("la cabecera no vale");
    quiebre = quiebre_inicio = (vacio_fin + 4095) & ~4095u;

    /* el arranque de 32 bits, al lado de este programa */
    i = readlink("/proc/self/exe", exe, sizeof exe - 16);
    if (i < 0) muere("no sé dónde estoy");
    exe[i] = 0;
    *strrchr(exe, '/') = 0;
    strcat(exe, "/arranque32");
    arranque = exe;

    hijo = fork();
    if (hijo == 0) {
        ptrace(PTRACE_TRACEME, 0, 0, 0);
        execl(arranque, "arranque32", (char *)NULL);
        _exit(122);
    }
    if (waitpid(hijo, &st, 0) < 0 || !WIFSTOPPED(st)) muere("el arranque no ha empezado");
    ptrace(PTRACE_SETOPTIONS, hijo, 0, PTRACE_O_TRACESYSGOOD | PTRACE_O_EXITKILL);
    snprintf(ruta, sizeof ruta, "/proc/%d/mem", hijo);
    mem = open(ruta, O_RDWR);
    if (mem < 0) muere("no puedo abrir la memoria del programa");
    ptrace(PTRACE_CONT, hijo, 0, 0);
    for (;;) {
        int sig;
        if (waitpid(hijo, &st, 0) < 0) muere("se ha perdido el programa");
        if (WIFEXITED(st)) {
            if (!preparado) muere("el arranque no ha podido preparar la memoria");
            return WEXITSTATUS(st);
        }
        if (WIFSIGNALED(st)) muere("el programa ha muerto");
        sig = WSTOPSIG(st);
        if (sig == SIGTRAP && !preparado) {
            /* el arranque ha preparado la memoria: el programa, a su sitio */
            if (!pon_mem(BASE, prog, largo)) muere("no puedo poner el programa en su sitio");
            ptrace(PTRACE_GETREGS, hijo, 0, &rg);
            rg.rip = entrada;
            rg.rsp = 0xC0000000u - 16;
            rg.rax = rg.rbx = rg.rcx = rg.rdx = rg.rsi = rg.rdi = rg.rbp = 0;
            ptrace(PTRACE_SETREGS, hijo, 0, &rg);
            preparado = 1;
            ptrace(PTRACE_SYSCALL, hijo, 0, 0);
            continue;
        }
        if (sig == (SIGTRAP | 0x80) && preparado) {
            /* entra en una llamada: se anula y se hace aquí */
            uint32_t res;
            int acaba = 0;
            ptrace(PTRACE_GETREGS, hijo, 0, &rg);
            res = atender((uint32_t)rg.orig_rax, (uint32_t)rg.rbx, (uint32_t)rg.rcx, (uint32_t)rg.rdx, &acaba);
            if (traza) fprintf(stderr, "[%u %x %x %x -> %x]\n", (uint32_t)rg.orig_rax, (uint32_t)rg.rbx, (uint32_t)rg.rcx, (uint32_t)rg.rdx, res);
            if (acaba) { fflush(stdout); kill(hijo, SIGKILL); waitpid(hijo, &st, 0); return (int)(res & 255); }
            rg.orig_rax = (uint64_t)-1;
            ptrace(PTRACE_SETREGS, hijo, 0, &rg);
            ptrace(PTRACE_SYSCALL, hijo, 0, 0);
            if (waitpid(hijo, &st, 0) < 0 || !WIFSTOPPED(st)) muere("se ha perdido el programa en una llamada");
            ptrace(PTRACE_GETREGS, hijo, 0, &rg);
            rg.rax = res;
            ptrace(PTRACE_SETREGS, hijo, 0, &rg);
            ptrace(PTRACE_SYSCALL, hijo, 0, 0);
            continue;
        }
        if (preparado && (sig == SIGSEGV || sig == SIGILL || sig == SIGFPE || sig == SIGBUS || sig == SIGTRAP)) {
            ptrace(PTRACE_GETREGS, hijo, 0, &rg);
            fprintf(stderr, "corre32: el programa ha fallado (señal %d) en %08x\n", sig, (uint32_t)rg.rip);
            fflush(stdout);
            kill(hijo, SIGKILL);
            return 255;
        }
        ptrace(preparado ? PTRACE_SYSCALL : PTRACE_CONT, hijo, 0, (void *)(long)(sig == SIGTRAP || sig == (SIGTRAP | 0x80) ? 0 : sig));
    }
}
