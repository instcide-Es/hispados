/* =============================================================================
 *  Hispa-DOS - utils/firma/cripto16.c: SHA-256 y la comprobación de una firma
 *  Ed25519 (RFC 8032) para ACTUALIZAR (ACTUALIZ.COM, modo real del 386).
 *  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 *
 *  Se compila con «gcc -m16» (código de 32 bits con prefijos, para el modo
 *  real o V86 de un 386 o superior) y se enlaza en una dirección fija dentro
 *  del segmento de ACTUALIZ.COM (cripto16.ld): ACTUALIZAR lo mete con incbin
 *  y lo llama con «o32 call» (convención cdecl, argumentos de 32 bits en la
 *  pila). Todo lo que toca tiene que estar por debajo de 64 KB (CS = DS = ES
 *  = SS) y la parte alta de ESP, a cero.
 *
 *  Las cuentas de la curva son las de TweetNaCl (dominio público: Bernstein,
 *  van Gastel, Janssen, Lange, Schwabe, Smetsers), con los productos de 32x32
 *  bits (los trozos de 16 bits caben de sobra en 32). Solo comprueba: aquí
 *  no hay nada secreto, así que no hace falta ir en tiempo constante.
 *
 *  Lo mismo se compila también para el anfitrión (tests/firma/), con las
 *  pruebas de la RFC 8032 y de NIST, y firmas hechas con Python.
 * ============================================================================= */

typedef unsigned char u8;
typedef unsigned int u32;
typedef int i32;
typedef unsigned long long u64;
typedef long long i64;
typedef i64 gf[16];

#define EXPORTA __attribute__((used, noinline))

/* gcc puede llamarlas para copiar o poner a cero estructuras (en el
   anfitrión, las pruebas usan las de la biblioteca de C) */
#ifndef CRIPTO_ANFITRION
void *memcpy(void *d, const void *s, unsigned long n)
{
    u8 *a = d; const u8 *b = s;
    while (n--) *a++ = *b++;
    return d;
}
void *memset(void *d, int c, unsigned long n)
{
    u8 *a = d;
    while (n--) *a++ = (u8)c;
    return d;
}
#endif

/* ---------------------------------------------------------------------------
 *  Las constantes (las sacó un guion de las raíces de los primos)
 * --------------------------------------------------------------------------- */
static const u32 K256[64] = {
    0x428A2F98, 0x71374491, 0xB5C0FBCF, 0xE9B5DBA5,
    0x3956C25B, 0x59F111F1, 0x923F82A4, 0xAB1C5ED5,
    0xD807AA98, 0x12835B01, 0x243185BE, 0x550C7DC3,
    0x72BE5D74, 0x80DEB1FE, 0x9BDC06A7, 0xC19BF174,
    0xE49B69C1, 0xEFBE4786, 0x0FC19DC6, 0x240CA1CC,
    0x2DE92C6F, 0x4A7484AA, 0x5CB0A9DC, 0x76F988DA,
    0x983E5152, 0xA831C66D, 0xB00327C8, 0xBF597FC7,
    0xC6E00BF3, 0xD5A79147, 0x06CA6351, 0x14292967,
    0x27B70A85, 0x2E1B2138, 0x4D2C6DFC, 0x53380D13,
    0x650A7354, 0x766A0ABB, 0x81C2C92E, 0x92722C85,
    0xA2BFE8A1, 0xA81A664B, 0xC24B8B70, 0xC76C51A3,
    0xD192E819, 0xD6990624, 0xF40E3585, 0x106AA070,
    0x19A4C116, 0x1E376C08, 0x2748774C, 0x34B0BCB5,
    0x391C0CB3, 0x4ED8AA4A, 0x5B9CCA4F, 0x682E6FF3,
    0x748F82EE, 0x78A5636F, 0x84C87814, 0x8CC70208,
    0x90BEFFFA, 0xA4506CEB, 0xBEF9A3F7, 0xC67178F2,
};
static const u32 H256[8] = {
    0x6A09E667, 0xBB67AE85, 0x3C6EF372, 0xA54FF53A,
    0x510E527F, 0x9B05688C, 0x1F83D9AB, 0x5BE0CD19,
};
static const u64 K512[80] = {
    0x428A2F98D728AE22ULL, 0x7137449123EF65CDULL, 0xB5C0FBCFEC4D3B2FULL, 0xE9B5DBA58189DBBCULL,
    0x3956C25BF348B538ULL, 0x59F111F1B605D019ULL, 0x923F82A4AF194F9BULL, 0xAB1C5ED5DA6D8118ULL,
    0xD807AA98A3030242ULL, 0x12835B0145706FBEULL, 0x243185BE4EE4B28CULL, 0x550C7DC3D5FFB4E2ULL,
    0x72BE5D74F27B896FULL, 0x80DEB1FE3B1696B1ULL, 0x9BDC06A725C71235ULL, 0xC19BF174CF692694ULL,
    0xE49B69C19EF14AD2ULL, 0xEFBE4786384F25E3ULL, 0x0FC19DC68B8CD5B5ULL, 0x240CA1CC77AC9C65ULL,
    0x2DE92C6F592B0275ULL, 0x4A7484AA6EA6E483ULL, 0x5CB0A9DCBD41FBD4ULL, 0x76F988DA831153B5ULL,
    0x983E5152EE66DFABULL, 0xA831C66D2DB43210ULL, 0xB00327C898FB213FULL, 0xBF597FC7BEEF0EE4ULL,
    0xC6E00BF33DA88FC2ULL, 0xD5A79147930AA725ULL, 0x06CA6351E003826FULL, 0x142929670A0E6E70ULL,
    0x27B70A8546D22FFCULL, 0x2E1B21385C26C926ULL, 0x4D2C6DFC5AC42AEDULL, 0x53380D139D95B3DFULL,
    0x650A73548BAF63DEULL, 0x766A0ABB3C77B2A8ULL, 0x81C2C92E47EDAEE6ULL, 0x92722C851482353BULL,
    0xA2BFE8A14CF10364ULL, 0xA81A664BBC423001ULL, 0xC24B8B70D0F89791ULL, 0xC76C51A30654BE30ULL,
    0xD192E819D6EF5218ULL, 0xD69906245565A910ULL, 0xF40E35855771202AULL, 0x106AA07032BBD1B8ULL,
    0x19A4C116B8D2D0C8ULL, 0x1E376C085141AB53ULL, 0x2748774CDF8EEB99ULL, 0x34B0BCB5E19B48A8ULL,
    0x391C0CB3C5C95A63ULL, 0x4ED8AA4AE3418ACBULL, 0x5B9CCA4F7763E373ULL, 0x682E6FF3D6B2B8A3ULL,
    0x748F82EE5DEFB2FCULL, 0x78A5636F43172F60ULL, 0x84C87814A1F0AB72ULL, 0x8CC702081A6439ECULL,
    0x90BEFFFA23631E28ULL, 0xA4506CEBDE82BDE9ULL, 0xBEF9A3F7B2C67915ULL, 0xC67178F2E372532BULL,
    0xCA273ECEEA26619CULL, 0xD186B8C721C0C207ULL, 0xEADA7DD6CDE0EB1EULL, 0xF57D4F7FEE6ED178ULL,
    0x06F067AA72176FBAULL, 0x0A637DC5A2C898A6ULL, 0x113F9804BEF90DAEULL, 0x1B710B35131C471BULL,
    0x28DB77F523047D84ULL, 0x32CAAB7B40C72493ULL, 0x3C9EBE0A15C9BEBCULL, 0x431D67C49C100D4CULL,
    0x4CC5D4BECB3E42B6ULL, 0x597F299CFC657E2AULL, 0x5FCB6FAB3AD6FAECULL, 0x6C44198C4A475817ULL,
};
static const u64 H512[8] = {
    0x6A09E667F3BCC908ULL, 0xBB67AE8584CAA73BULL, 0x3C6EF372FE94F82BULL, 0xA54FF53A5F1D36F1ULL,
    0x510E527FADE682D1ULL, 0x9B05688C2B3E6C1FULL, 0x1F83D9ABFB41BD6BULL, 0x5BE0CD19137E2179ULL,
};

/* ---------------------------------------------------------------------------
 *  SHA-256 (FIPS 180-4), por trozos: cr_sha_ini, cr_sha_mas, cr_sha_fin
 * --------------------------------------------------------------------------- */
static u32 s2_h[8];
static u8 s2_blq[64];
static u32 s2_en;          /* octetos en s2_blq */
static u32 s2_tot_a, s2_tot_b;   /* octetos en total: alta y baja */

#define ROR32(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static void s2_bloque(const u8 *p)
{
    u32 w[64], a, b, c, d, e, f, g, h, t1, t2;
    int i;
    for (i = 0; i < 16; i++)
        w[i] = ((u32)p[4*i] << 24) | ((u32)p[4*i+1] << 16) | ((u32)p[4*i+2] << 8) | p[4*i+3];
    for (i = 16; i < 64; i++) {
        u32 s0 = ROR32(w[i-15], 7) ^ ROR32(w[i-15], 18) ^ (w[i-15] >> 3);
        u32 s1 = ROR32(w[i-2], 17) ^ ROR32(w[i-2], 19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    a = s2_h[0]; b = s2_h[1]; c = s2_h[2]; d = s2_h[3];
    e = s2_h[4]; f = s2_h[5]; g = s2_h[6]; h = s2_h[7];
    for (i = 0; i < 64; i++) {
        t1 = h + (ROR32(e, 6) ^ ROR32(e, 11) ^ ROR32(e, 25)) + ((e & f) ^ (~e & g)) + K256[i] + w[i];
        t2 = (ROR32(a, 2) ^ ROR32(a, 13) ^ ROR32(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    s2_h[0] += a; s2_h[1] += b; s2_h[2] += c; s2_h[3] += d;
    s2_h[4] += e; s2_h[5] += f; s2_h[6] += g; s2_h[7] += h;
}

EXPORTA void cr_sha_ini(void)
{
    int i;
    for (i = 0; i < 8; i++) s2_h[i] = H256[i];
    s2_en = 0; s2_tot_a = 0; s2_tot_b = 0;
}

EXPORTA void cr_sha_mas(const u8 *p, u32 n)
{
    u32 viejo = s2_tot_b;
    s2_tot_b += n;
    if (s2_tot_b < viejo) s2_tot_a++;
    while (n) {
        if (s2_en == 0 && n >= 64) {
            s2_bloque(p);
            p += 64; n -= 64;
            continue;
        }
        s2_blq[s2_en++] = *p++;
        n--;
        if (s2_en == 64) { s2_bloque(s2_blq); s2_en = 0; }
    }
}

EXPORTA void cr_sha_fin(u8 *out)
{
    u32 ba = (s2_tot_a << 3) | (s2_tot_b >> 29), bb = s2_tot_b << 3;
    int i;
    s2_blq[s2_en++] = 0x80;
    if (s2_en > 56) {
        while (s2_en < 64) s2_blq[s2_en++] = 0;
        s2_bloque(s2_blq);
        s2_en = 0;
    }
    while (s2_en < 56) s2_blq[s2_en++] = 0;
    for (i = 0; i < 4; i++) { s2_blq[56+i] = (u8)(ba >> (24 - 8*i)); s2_blq[60+i] = (u8)(bb >> (24 - 8*i)); }
    s2_bloque(s2_blq);
    for (i = 0; i < 8; i++) {
        out[4*i] = (u8)(s2_h[i] >> 24); out[4*i+1] = (u8)(s2_h[i] >> 16);
        out[4*i+2] = (u8)(s2_h[i] >> 8); out[4*i+3] = (u8)s2_h[i];
    }
}

/* ---------------------------------------------------------------------------
 *  SHA-512, por trozos (para Ed25519; mensajes de menos de 4 GB)
 * --------------------------------------------------------------------------- */
static u64 s5_h[8];
static u8 s5_blq[128];
static u32 s5_en, s5_tot;

#define ROR64(x, n) (((x) >> (n)) | ((x) << (64 - (n))))

static void s5_bloque(const u8 *p)
{
    u64 w[80], v[8], t1, t2;
    int i, j;
    for (i = 0; i < 16; i++) {
        u64 x = 0;
        for (j = 0; j < 8; j++) x = (x << 8) | p[8*i+j];
        w[i] = x;
    }
    for (i = 16; i < 80; i++) {
        u64 s0 = ROR64(w[i-15], 1) ^ ROR64(w[i-15], 8) ^ (w[i-15] >> 7);
        u64 s1 = ROR64(w[i-2], 19) ^ ROR64(w[i-2], 61) ^ (w[i-2] >> 6);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    for (i = 0; i < 8; i++) v[i] = s5_h[i];
    for (i = 0; i < 80; i++) {
        t1 = v[7] + (ROR64(v[4], 14) ^ ROR64(v[4], 18) ^ ROR64(v[4], 41)) + ((v[4] & v[5]) ^ (~v[4] & v[6])) + K512[i] + w[i];
        t2 = (ROR64(v[0], 28) ^ ROR64(v[0], 34) ^ ROR64(v[0], 39)) + ((v[0] & v[1]) ^ (v[0] & v[2]) ^ (v[1] & v[2]));
        v[7] = v[6]; v[6] = v[5]; v[5] = v[4]; v[4] = v[3] + t1;
        v[3] = v[2]; v[2] = v[1]; v[1] = v[0]; v[0] = t1 + t2;
    }
    for (i = 0; i < 8; i++) s5_h[i] += v[i];
}

static void s5_ini(void)
{
    int i;
    for (i = 0; i < 8; i++) s5_h[i] = H512[i];
    s5_en = 0; s5_tot = 0;
}

static void s5_mas(const u8 *p, u32 n)
{
    s5_tot += n;
    while (n--) {
        s5_blq[s5_en++] = *p++;
        if (s5_en == 128) { s5_bloque(s5_blq); s5_en = 0; }
    }
}

static void s5_fin(u8 *out)
{
    int i;
    s5_blq[s5_en++] = 0x80;
    if (s5_en > 112) {
        while (s5_en < 128) s5_blq[s5_en++] = 0;
        s5_bloque(s5_blq);
        s5_en = 0;
    }
    while (s5_en < 124) s5_blq[s5_en++] = 0;
    s5_blq[124] = (u8)(s5_tot >> 21); s5_blq[125] = (u8)(s5_tot >> 13);
    s5_blq[126] = (u8)(s5_tot >> 5);  s5_blq[127] = (u8)(s5_tot << 3);
    s5_bloque(s5_blq);
    for (i = 0; i < 64; i++) out[i] = (u8)(s5_h[i/8] >> (56 - 8*(i%8)));
}

/* ---------------------------------------------------------------------------
 *  La curva (TweetNaCl)
 * --------------------------------------------------------------------------- */
static const gf gf0;
static const gf gf1 = {1};
static const gf D = {0x78a3, 0x1359, 0x4dca, 0x75eb, 0xd8ab, 0x4141, 0x0a4d, 0x0070,
                     0xe898, 0x7779, 0x4079, 0x8cc7, 0xfe73, 0x2b6f, 0x6cee, 0x5203};
static const gf D2 = {0xf159, 0x26b2, 0x9b94, 0xebd6, 0xb156, 0x8283, 0x149a, 0x00e0,
                      0xd130, 0xeef3, 0x80f2, 0x198e, 0xfce7, 0x56df, 0xd9dc, 0x2406};
static const gf X = {0xd51a, 0x8f25, 0x2d60, 0xc956, 0xa7b2, 0x9525, 0xc760, 0x692c,
                     0xdc5c, 0xfdd6, 0xe231, 0xc0a4, 0x53fe, 0xcd6e, 0x36d3, 0x2169};
static const gf Y = {0x6658, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666,
                     0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666};
static const gf I = {0xa0b0, 0x4a0e, 0x1b27, 0xc4ee, 0xe478, 0xad2f, 0x1806, 0x2f43,
                     0xd7a7, 0x3dfb, 0x0099, 0x2b4d, 0xdf0b, 0x4fc1, 0x2480, 0x2b83};

static void set25519(gf r, const gf a)
{
    int i;
    for (i = 0; i < 16; i++) r[i] = a[i];
}

static void car25519(gf o)
{
    int i;
    i64 c;
    for (i = 0; i < 16; i++) {
        o[i] += 65536;
        c = o[i] >> 16;
        if (i < 15) o[i+1] += c - 1;
        else o[0] += 38 * (c - 1);
        o[i] -= c * 65536;
    }
}

static void sel25519(gf p, gf q, int b)
{
    i64 t, c = ~((i64)b - 1);
    int i;
    for (i = 0; i < 16; i++) {
        t = c & (p[i] ^ q[i]);
        p[i] ^= t;
        q[i] ^= t;
    }
}

static void pack25519(u8 *o, const gf n)
{
    int i, j, b;
    gf m, t;
    for (i = 0; i < 16; i++) t[i] = n[i];
    car25519(t);
    car25519(t);
    car25519(t);
    for (j = 0; j < 2; j++) {
        m[0] = t[0] - 0xffed;
        for (i = 1; i < 15; i++) {
            m[i] = t[i] - 0xffff - ((m[i-1] >> 16) & 1);
            m[i-1] &= 0xffff;
        }
        m[15] = t[15] - 0x7fff - ((m[14] >> 16) & 1);
        b = (int)((m[15] >> 16) & 1);
        m[14] &= 0xffff;
        sel25519(t, m, 1 - b);
    }
    for (i = 0; i < 16; i++) {
        o[2*i] = (u8)(t[i] & 0xff);
        o[2*i+1] = (u8)(t[i] >> 8);
    }
}

static int iguales(const u8 *x, const u8 *y, int n)
{
    u8 d = 0;
    int i;
    for (i = 0; i < n; i++) d |= x[i] ^ y[i];
    return d == 0;
}

static int neq25519(const gf a, const gf b)
{
    u8 c[32], d[32];
    pack25519(c, a);
    pack25519(d, b);
    return !iguales(c, d, 32);
}

static u8 par25519(const gf a)
{
    u8 d[32];
    pack25519(d, a);
    return d[0] & 1;
}

static void unpack25519(gf o, const u8 *n)
{
    int i;
    for (i = 0; i < 16; i++) o[i] = n[2*i] + ((i64)n[2*i+1] << 8);
    o[15] &= 0x7fff;
}

static void A(gf o, const gf a, const gf b)
{
    int i;
    for (i = 0; i < 16; i++) o[i] = a[i] + b[i];
}

static void Z(gf o, const gf a, const gf b)
{
    int i;
    for (i = 0; i < 16; i++) o[i] = a[i] - b[i];
}

/* los trozos que entran caben en 32 bits (de car25519, o la suma o la resta
   de dos así): cada producto, un solo imul de 32x32 -> 64 */
static void M(gf o, const gf a, const gf b)
{
    i64 t[31];
    int i, j;
    for (i = 0; i < 31; i++) t[i] = 0;
    for (i = 0; i < 16; i++) {
        i32 ai = (i32)a[i];
        for (j = 0; j < 16; j++) t[i+j] += (i64)ai * (i32)b[j];
    }
    for (i = 0; i < 15; i++) t[i] += 38 * t[i+16];
    for (i = 0; i < 16; i++) o[i] = t[i];
    car25519(o);
    car25519(o);
}

static void S(gf o, const gf a)
{
    M(o, a, a);
}

static void inv25519(gf o, const gf i)
{
    gf c;
    int a;
    for (a = 0; a < 16; a++) c[a] = i[a];
    for (a = 253; a >= 0; a--) {
        S(c, c);
        if (a != 2 && a != 4) M(c, c, i);
    }
    for (a = 0; a < 16; a++) o[a] = c[a];
}

static void pow2523(gf o, const gf i)
{
    gf c;
    int a;
    for (a = 0; a < 16; a++) c[a] = i[a];
    for (a = 250; a >= 0; a--) {
        S(c, c);
        if (a != 1) M(c, c, i);
    }
    for (a = 0; a < 16; a++) o[a] = c[a];
}

static void suma(gf p[4], gf q[4])
{
    gf a, b, c, d, t, e, f, g, h;
    Z(a, p[1], p[0]);
    Z(t, q[1], q[0]);
    M(a, a, t);
    A(b, p[0], p[1]);
    A(t, q[0], q[1]);
    M(b, b, t);
    M(c, p[3], q[3]);
    M(c, c, D2);
    M(d, p[2], q[2]);
    A(d, d, d);
    Z(e, b, a);
    Z(f, d, c);
    A(g, d, c);
    A(h, b, a);
    M(p[0], e, f);
    M(p[1], h, g);
    M(p[2], g, f);
    M(p[3], e, h);
}

static void cswap(gf p[4], gf q[4], u8 b)
{
    int i;
    for (i = 0; i < 4; i++) sel25519(p[i], q[i], b);
}

static void pack(u8 *r, gf p[4])
{
    gf tx, ty, zi;
    inv25519(zi, p[2]);
    M(tx, p[0], zi);
    M(ty, p[1], zi);
    pack25519(r, ty);
    r[31] ^= par25519(tx) << 7;
}

static void scalarmult(gf p[4], gf q[4], const u8 *s)
{
    int i;
    set25519(p[0], gf0);
    set25519(p[1], gf1);
    set25519(p[2], gf1);
    set25519(p[3], gf0);
    for (i = 255; i >= 0; --i) {
        u8 b = (s[i/8] >> (i & 7)) & 1;
        cswap(p, q, b);
        suma(q, p);
        suma(p, p);
        cswap(p, q, b);
    }
}

static void scalarbase(gf p[4], const u8 *s)
{
    gf q[4];
    set25519(q[0], X);
    set25519(q[1], Y);
    set25519(q[2], gf1);
    M(q[3], X, Y);
    scalarmult(p, q, s);
}

static const i64 L[32] = {0xed, 0xd3, 0xf5, 0x5c, 0x1a, 0x63, 0x12, 0x58,
                          0xd6, 0x9c, 0xf7, 0xa2, 0xde, 0xf9, 0xde, 0x14,
                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x10};

static void modL(u8 *r, i64 x[64])
{
    i64 carry;
    int i, j;
    for (i = 63; i >= 32; --i) {
        carry = 0;
        for (j = i - 32; j < i - 12; ++j) {
            x[j] += carry - 16 * x[i] * L[j - (i - 32)];
            carry = (x[j] + 128) >> 8;
            x[j] -= carry * 256;
        }
        x[j] += carry;
        x[i] = 0;
    }
    carry = 0;
    for (j = 0; j < 32; j++) {
        x[j] += carry - (x[31] >> 4) * L[j];
        carry = x[j] >> 8;
        x[j] &= 255;
    }
    for (j = 0; j < 32; j++) x[j] -= carry * L[j];
    for (i = 0; i < 32; i++) {
        x[i+1] += x[i] >> 8;
        r[i] = (u8)(x[i] & 255);
    }
}

static void reduce(u8 *r)
{
    i64 x[64];
    int i;
    for (i = 0; i < 64; i++) x[i] = r[i];
    for (i = 0; i < 64; i++) r[i] = 0;
    modL(r, x);
}

static int unpackneg(gf r[4], const u8 p[32])
{
    gf t, chk, num, den, den2, den4, den6;
    set25519(r[2], gf1);
    unpack25519(r[1], p);
    S(num, r[1]);
    M(den, num, D);
    Z(num, num, r[2]);
    A(den, r[2], den);
    S(den2, den);
    S(den4, den2);
    M(den6, den4, den2);
    M(t, den6, num);
    M(t, t, den);
    pow2523(t, t);
    M(t, t, num);
    M(t, t, den);
    M(t, t, den);
    M(r[0], t, den);
    S(chk, r[0]);
    M(chk, chk, den);
    if (neq25519(chk, num)) M(r[0], r[0], I);
    S(chk, r[0]);
    M(chk, chk, den);
    if (neq25519(chk, num)) return -1;
    if (par25519(r[0]) == (p[31] >> 7)) Z(r[0], gf0, r[0]);
    M(r[3], r[0], r[1]);
    return 0;
}

/* S < L (RFC 8032, 5.1.7: si no, la firma no vale; así no hay dos firmas
   buenas para lo mismo) */
static int s_menor_que_l(const u8 *s)
{
    int i;
    for (i = 31; i >= 0; i--) {
        if (s[i] < (u8)L[i]) return 1;
        if (s[i] > (u8)L[i]) return 0;
    }
    return 0;           /* igual a L: no vale */
}

/* ---------------------------------------------------------------------------
 *  cr_firma(firma, pre, npre, msj, n, clave) -> 1 si la firma Ed25519
 *  (64 octetos: R y S) de pre||msj es buena con la clave pública (32
 *  octetos); 0 si no. «pre» es la etiqueta de lo que se firma (para que una
 *  firma de otra cosa —un catálogo de Hispa-Pentium— no valga aquí).
 * --------------------------------------------------------------------------- */
EXPORTA int cr_firma(const u8 *firma, const u8 *pre, u32 npre, const u8 *msj, u32 n, const u8 *clave)
{
    static gf p[4], q[4];
    u8 t[32], h[64];
    if (!s_menor_que_l(firma + 32)) return 0;
    if (unpackneg(q, clave)) return 0;
    s5_ini();
    s5_mas(firma, 32);
    s5_mas(clave, 32);
    s5_mas(pre, npre);
    s5_mas(msj, n);
    s5_fin(h);
    reduce(h);
    scalarmult(p, q, h);
    scalarbase(q, firma + 32);
    suma(p, q);
    pack(t, p);
    return iguales(firma, t, 32);
}
