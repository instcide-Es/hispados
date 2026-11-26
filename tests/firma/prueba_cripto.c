/* Hispa-DOS - tests/firma/prueba_cripto.c: las cuentas de utils/firma/cripto16.c
 * (SHA-256 y Ed25519 de ACTUALIZAR), en el anfitrión, contra lo que dice
 * Python (hashlib y cryptography): lee de la entrada líneas
 *   sha <datos hex | -> <resumen hex> [trozo]
 *   ed <clave> <firma> <prefijo hex | -> <mensaje hex | -> <0|1>
 * y dice OK/FAIL de cada una.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CRIPTO_ANFITRION 1
#include "../../utils/firma/cripto16.c"

static int hex(const char *s, u8 *o, int max)
{
    int n = 0;
    if (!strcmp(s, "-")) return 0;
    while (s[0] && s[1] && n < max) {
        unsigned v;
        sscanf(s, "%2x", &v);
        o[n++] = (u8)v;
        s += 2;
    }
    return n;
}

int main(void)
{
    static char lin[200000];
    static u8 dat[90000];
    int ok = 0, mal = 0, num = 0;
    while (fgets(lin, sizeof lin, stdin)) {
        char *t[8];
        int nt = 0;
        char *p = strtok(lin, " \r\n");
        while (p && nt < 8) { t[nt++] = p; p = strtok(NULL, " \r\n"); }
        if (nt == 0) continue;
        num++;
        if (!strcmp(t[0], "sha") && nt >= 3) {
            u8 r[32], e[32];
            int n = hex(t[1], dat, sizeof dat), trozo = nt > 3 ? atoi(t[3]) : n, i;
            hex(t[2], e, 32);
            cr_sha_ini();
            for (i = 0; i < n; i += trozo) cr_sha_mas(dat + i, (u32)(n - i < trozo ? n - i : trozo));
            if (n == 0) cr_sha_mas(dat, 0);
            cr_sha_fin(r);
            if (!memcmp(r, e, 32)) ok++; else { mal++; printf("FAIL sha linea %d (%d octetos)\n", num, n); }
        } else if (!strcmp(t[0], "ed") && nt >= 6) {
            u8 k[32], f[64], pre[256];
            int np, n, quiere = atoi(t[5]), da;
            hex(t[1], k, 32);
            hex(t[2], f, 64);
            np = hex(t[3], pre, sizeof pre);
            n = hex(t[4], dat, sizeof dat);
            da = cr_firma(f, pre, (u32)np, dat, (u32)n, k);
            if (da == quiere) ok++; else { mal++; printf("FAIL ed linea %d: da %d, tiene que dar %d\n", num, da, quiere); }
        }
    }
    printf("cripto16 (anfitrión): %d OK, %d FAIL\n", ok, mal);
    return mal != 0;
}
