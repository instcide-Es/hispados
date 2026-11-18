/* =============================================================================
 *  HSPTLS.DLL de Hispa-486: conexiones cifradas (TLS 1.2 de cliente) para los
 *  programas Hispa32: el navegador, HTTPS.EXE (docs/RED.md).
 *  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 *
 *  La criptografía y el protocolo son de BearSSL 0.6 (Thomas Pornin, licencia
 *  MIT: terceros/bearssl), hecho para máquinas pequeñas: sin memoria dinámica
 *  y con implementaciones de 32 bits. Aquí va lo nuestro:
 *   - el perfil: solo TLS 1.2, intercambio ECDHE (Curve25519, P-256, P-384),
 *     servidor con certificado ECDSA o RSA, y ChaCha20-Poly1305 o AES-GCM;
 *   - la comprobación del certificado: cadena hasta una de las autoridades
 *     raíz de anclas.h (hechas con tools/mkanclas.sh de w32/tls/raices) o de
 *     los archivos *.CER (X.509 en DER) que haya en la carpeta del programa,
 *     nombre del servidor y fechas con el reloj del equipo;
 *   - la memoria de certificados: comprobar la cadena de clicgest.es (dos
 *     firmas ECDSA P-384) le cuesta a un 486 a 66 MHz unos 13 segundos, así
 *     que la cadena que ya se dio por buena para un servidor (su resumen
 *     SHA-256, con el nombre) se recuerda una semana (HSPTLS.CCH, junto al
 *     programa) y mientras tanto no se vuelve a comprobar: el apretón de
 *     manos baja a unos 2 segundos. Si el servidor cambia de certificado, o
 *     pasa la semana, se comprueba otra vez entera;
 *   - el azar: no hay generador del sistema, así que se junta lo imprevisible
 *     que hay (el contador del reloj —PIT— y el de ciclos —TSC, si lo hay—
 *     cada vez que llega algo de la red o se llama aquí, la hora, lo que
 *     quiera añadir el programa con TlsAzar: teclas, ratón) y una semilla que
 *     se guarda de una vez para otra (HSPTLS.SEM, junto al programa);
 *   - el paso de los registros por un socket de WSOCK32.DLL sin bloqueo.
 *
 *  Las funciones (stdcall). Devuelven TLS_ESPERA (-2) si aún no se puede y
 *  -1 si la conexión ha fallado (TlsError dice por qué):
 *    TlsAbrir(socket, servidor)         -> la conexión (NULL: sin memoria)
 *    TlsConectar(t, ms)                 -> 1 hecho el apretón de manos
 *    TlsLeer(t, búfer, n, ms)           -> octetos; 0 = el servidor cerró
 *    TlsEscribir(t, datos, n)           -> n (espera lo que haga falta)
 *    TlsCerrar(t)                       (no cierra el socket)
 *    TlsError(t), TlsTextoError(código), TlsInfo(t, texto, n), TlsAzar(d, n)
 *    TlsCiclos(t)  -> lo que lleva gastado el procesador en cifrar y en el
 *                     apretón de manos, en unidades de 1024 ciclos (por el
 *                     TSC; 0 si no lo hay): para medir
 *  «ms»: lo que se espera como mucho; 0 = nada (para programas con ventana,
 *  que vuelven a llamar), -1 = lo que haga falta.
 * ============================================================================= */
#include "bearssl.h"
#include "anclas.h"

typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;
typedef void *HANDLE;
#define WINAPI __attribute__((stdcall))
#define TLS_ESPERA (-2)

/* WSOCK32 y KERNEL32 */
int WINAPI send(u32, const char *, int, int);
int WINAPI recv(u32, char *, int, int);
int WINAPI ioctlsocket(u32, u32, u32 *);
int WINAPI WSAGetLastError(void);
void WINAPI Sleep(u32);
u32 WINAPI GetTickCount(void);
typedef struct { u16 anio, mes, sem, dia, hora, min, seg, ms; } ST;
void WINAPI GetLocalTime(ST *);
u32 WINAPI GetModuleFileNameA(HANDLE, char *, u32);
HANDLE WINAPI CreateFileA(const char *, u32, u32, void *, u32, u32, HANDLE);
int WINAPI ReadFile(HANDLE, void *, u32, u32 *, void *);
int WINAPI WriteFile(HANDLE, const void *, u32, u32 *, void *);
int WINAPI CloseHandle(HANDLE);
typedef struct { u32 atrib, c[2], a[2], e[2], tam_hi, tam_lo, r0, r1; char nombre[260]; char alt[14]; } FINDA;
HANDLE WINAPI FindFirstFileA(const char *, FINDA *);
int WINAPI FindNextFileA(HANDLE, FINDA *);
int WINAPI FindClose(HANDLE);
void *malloc(size_t);
void free(void *);

#define SALIDA (4096 + 85)     /* el búfer de lo que se envía */
#define CAD_MAX 10240          /* la cadena de certificados, para recordarla */
#define CAD_N 8
typedef struct TLS {
  br_ssl_client_context cc;
  br_x509_minimal_context xc;
  const br_x509_class *xv;     /* lo que ve BearSSL: la memoria, delante */
  br_x509_decoder_context dc;
  u32 cad_n, cad_len[CAD_N], n_cert, dias;
  u8 directo, de_memoria;
  u32 s;
  int error, fin_tcp, hecho;
  unsigned long long ciclos;
  char servidor[64];
  u8 cadena[CAD_MAX];
  u8 entrada[BR_SSL_BUFSIZE_INPUT];
  u8 salida[SALIDA];
} TLS;

/* ---- el azar ---- */
static br_sha256_context pozo;
static int pozo_listo, hay_tsc;
static char carpeta[150];      /* la del programa, acabada en barra */

static u32 pit(void) {
  u8 a, b;
  __asm__ volatile("outb %%al, $0x43" : : "a"(0));
  __asm__ volatile("inb $0x40, %%al" : "=a"(a));
  __asm__ volatile("inb $0x40, %%al" : "=a"(b));
  return a | b << 8;
}
static int cpuid_tsc(void) {
  u32 a, b, c, d, f1, f2;
  __asm__ volatile("pushfl; popl %0; movl %0, %1; xorl $0x200000, %0; pushl %0; popfl; pushfl; popl %0; pushl %1; popfl"
                   : "=&r"(f1), "=&r"(f2));
  if (!((f1 ^ f2) & 0x200000)) return 0;       /* un 486 sin CPUID */
  __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(0));
  if (a < 1) return 0;
  __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(1));
  return (d >> 4) & 1;
}
static void ruido(void) {
  u32 v[4];
  v[0] = pit(); v[1] = GetTickCount(); v[2] = v[3] = 0;
  if (hay_tsc) __asm__ volatile("rdtsc" : "=a"(v[2]), "=d"(v[3]));
  br_sha256_update(&pozo, v, sizeof v);
}
static void pozo_inicia(void) {
  ST st;
  char ruta[170];
  u8 sem[32];
  u32 n = 0, k;
  HANDLE h;
  br_sha256_init(&pozo);
  hay_tsc = cpuid_tsc();
  GetLocalTime(&st);
  br_sha256_update(&pozo, &st, sizeof st);
  n = GetModuleFileNameA(NULL, carpeta, sizeof carpeta - 1);
  carpeta[n] = 0;
  for (k = n; k > 0 && carpeta[k - 1] != '\\' && carpeta[k - 1] != ':'; k--) ;
  carpeta[k] = 0;
  ruido();
  /* la semilla de la otra vez, y se deja una nueva para la siguiente */
  memcpy(ruta, carpeta, k);
  memcpy(ruta + k, "HSPTLS.SEM", 11);
  h = CreateFileA(ruta, 0x80000000u, 1, NULL, 3, 0, NULL);
  if (h != (HANDLE)-1) {
    n = 0;
    ReadFile(h, sem, sizeof sem, &n, NULL);
    CloseHandle(h);
    br_sha256_update(&pozo, sem, n);
  }
  ruido();
  br_sha256_update(&pozo, "semilla", 7);
  br_sha256_out(&pozo, sem);
  h = CreateFileA(ruta, 0x40000000u, 0, NULL, 2, 0x80, NULL);
  if (h != (HANDLE)-1) {
    WriteFile(h, sem, sizeof sem, &n, NULL);
    CloseHandle(h);
  }
  br_sha256_update(&pozo, "uso", 3);
  pozo_listo = 1;
}
void WINAPI TlsAzar(const void *d, int n) {
  if (!pozo_listo) pozo_inicia();
  if (d && n > 0) br_sha256_update(&pozo, d, n);
  ruido();
}

/* ---- las autoridades de más: los *.CER (DER) de la carpeta del programa ---- */
#define N_MAS 4
static br_x509_trust_anchor anclas[TAs_NUM + N_MAS];
static int n_anclas;
static u8 mas_dn[N_MAS][300], mas_clave[N_MAS][600];
static u32 dn_n;
static void dn_trozo(void *ctx, const void *d, size_t n) {
  u8 *b = ctx;
  if (dn_n + n <= 300) memcpy(b + dn_n, d, n);
  dn_n += n;
}
static void ancla_de_archivo(const char *ruta) {
  static u8 der[4096];
  br_x509_decoder_context dc;
  br_x509_pkey *pk;
  br_x509_trust_anchor *ta;
  u32 n = 0;
  int k = n_anclas - TAs_NUM;
  HANDLE h;
  if (k >= N_MAS) return;
  h = CreateFileA(ruta, 0x80000000u, 1, NULL, 3, 0, NULL);
  if (h == (HANDLE)-1) return;
  ReadFile(h, der, sizeof der, &n, NULL);
  CloseHandle(h);
  if (n < 64 || n == sizeof der) return;
  dn_n = 0;
  br_x509_decoder_init(&dc, dn_trozo, mas_dn[k]);
  br_x509_decoder_push(&dc, der, n);
  pk = br_x509_decoder_get_pkey(&dc);
  if (!pk || dn_n > 300 || !br_x509_decoder_isCA(&dc)) return;
  ta = &anclas[n_anclas];
  ta->dn.data = mas_dn[k]; ta->dn.len = dn_n;
  ta->flags = BR_X509_TA_CA;
  ta->pkey = *pk;
  if (pk->key_type == BR_KEYTYPE_RSA) {
    if (pk->key.rsa.nlen + pk->key.rsa.elen > 600) return;
    memcpy(mas_clave[k], pk->key.rsa.n, pk->key.rsa.nlen);
    memcpy(mas_clave[k] + pk->key.rsa.nlen, pk->key.rsa.e, pk->key.rsa.elen);
    ta->pkey.key.rsa.n = mas_clave[k];
    ta->pkey.key.rsa.e = mas_clave[k] + pk->key.rsa.nlen;
  } else if (pk->key_type == BR_KEYTYPE_EC) {
    if (pk->key.ec.qlen > 600) return;
    memcpy(mas_clave[k], pk->key.ec.q, pk->key.ec.qlen);
    ta->pkey.key.ec.q = mas_clave[k];
  } else return;
  n_anclas++;
}
static void anclas_inicia(void) {
  FINDA f;
  HANDLE h;
  char ruta[170];
  u32 k = strlen(carpeta);
  memcpy(anclas, TAs, sizeof TAs);
  n_anclas = TAs_NUM;
  memcpy(ruta, carpeta, k);
  memcpy(ruta + k, "*.CER", 6);
  h = FindFirstFileA(ruta, &f);
  if (h == (HANDLE)-1) return;
  do {
    u32 l = strlen(f.nombre);
    if (l < 13) { memcpy(ruta + k, f.nombre, l + 1); ancla_de_archivo(ruta); }
  } while (FindNextFileA(h, &f));
  FindClose(h);
}

/* ---- la memoria de certificados: delante de la comprobación de BearSSL.
 * Guarda la cadena según llega; al acabar, si su resumen (con el nombre del
 * servidor) está entre los que se dieron por buenos hace menos de una
 * semana, solo se lee la clave del primer certificado; si no, se le pasa
 * entera a la comprobación de verdad y, si vale, se apunta ---- */
#define MEM_N 16
#define MEM_DIAS 7
static struct { u8 h[32]; u32 dia; } memoria[MEM_N];
static int mem_leida, mem_sig;
#define TLS_DE(c) ((TLS *)((char *)(c) - offsetof(TLS, xv)))
static void mem_archivo(int escribir) {
  char ruta[170];
  u32 k = strlen(carpeta), n;
  HANDLE h;
  memcpy(ruta, carpeta, k);
  memcpy(ruta + k, "HSPTLS.CCH", 11);
  h = escribir ? CreateFileA(ruta, 0x40000000u, 0, NULL, 2, 0x80, NULL) : CreateFileA(ruta, 0x80000000u, 1, NULL, 3, 0, NULL);
  if (h == (HANDLE)-1) return;
  if (escribir) WriteFile(h, memoria, sizeof memoria, &n, NULL);
  else if (!ReadFile(h, memoria, sizeof memoria, &n, NULL) || n != sizeof memoria) memset(memoria, 0, sizeof memoria);
  CloseHandle(h);
}
static void mem_resumen(TLS *t, u8 *h) {
  br_sha256_context c;
  br_sha256_init(&c);
  br_sha256_update(&c, t->servidor, strlen(t->servidor) + 1);
  br_sha256_update(&c, t->cad_len, t->n_cert * sizeof t->cad_len[0]);
  br_sha256_update(&c, t->cadena, t->cad_n);
  br_sha256_out(&c, h);
}
/* lo guardado, a la comprobación de verdad */
static void mem_vuelca(TLS *t) {
  const br_x509_class **x = &t->xc.vtable;
  u32 k, o = 0;
  (*x)->start_chain(x, t->servidor);
  for (k = 0; k < t->n_cert; k++) {
    (*x)->start_cert(x, t->cad_len[k]);
    (*x)->append(x, t->cadena + o, t->cad_len[k]);
    (*x)->end_cert(x);
    o += t->cad_len[k];
  }
}
static void xm_start_chain(const br_x509_class **c, const char *servidor) {
  TLS *t = TLS_DE(c);
  (void)servidor;
  t->cad_n = 0; t->n_cert = 0; t->directo = 0; t->de_memoria = 0;
}
static void xm_start_cert(const br_x509_class **c, uint32_t n) {
  TLS *t = TLS_DE(c);
  if (!t->directo && (t->n_cert == CAD_N || t->cad_n + n > CAD_MAX)) {
    mem_vuelca(t);             /* no cabe: sin memoria, como siempre */
    t->directo = 1;
  }
  if (t->directo) t->xc.vtable->start_cert(&t->xc.vtable, n);
  else t->cad_len[t->n_cert] = n;
}
static void xm_append(const br_x509_class **c, const unsigned char *b, size_t n) {
  TLS *t = TLS_DE(c);
  if (t->directo) t->xc.vtable->append(&t->xc.vtable, b, n);
  else if (t->cad_n + n <= CAD_MAX) { memcpy(t->cadena + t->cad_n, b, n); t->cad_n += n; }
}
static void xm_end_cert(const br_x509_class **c) {
  TLS *t = TLS_DE(c);
  if (t->directo) t->xc.vtable->end_cert(&t->xc.vtable);
  else t->n_cert++;
}
static unsigned xm_end_chain(const br_x509_class **c) {
  TLS *t = TLS_DE(c);
  u8 h[32];
  unsigned r;
  int k;
  if (t->directo) return t->xc.vtable->end_chain(&t->xc.vtable);
  if (!t->n_cert) return BR_ERR_X509_EMPTY_CHAIN;
  if (!mem_leida) { mem_leida = 1; mem_archivo(0); }
  mem_resumen(t, h);
  for (k = 0; k < MEM_N; k++)
    if (memoria[k].dia && !memcmp(memoria[k].h, h, 32) && t->dias >= memoria[k].dia && t->dias < memoria[k].dia + MEM_DIAS) {
      br_x509_decoder_init(&t->dc, 0, 0);
      br_x509_decoder_push(&t->dc, t->cadena, t->cad_len[0]);
      if (br_x509_decoder_get_pkey(&t->dc)) { t->de_memoria = 1; return 0; }
    }
  mem_vuelca(t);
  r = t->xc.vtable->end_chain(&t->xc.vtable);
  if (r == 0) {
    for (k = 0; k < MEM_N && memcmp(memoria[k].h, h, 32); k++) ;
    if (k == MEM_N) k = mem_sig = (mem_sig + 1) % MEM_N;
    memcpy(memoria[k].h, h, 32);
    memoria[k].dia = t->dias;
    mem_archivo(1);
  }
  return r;
}
static const br_x509_pkey *xm_get_pkey(const br_x509_class *const *c, unsigned *usos) {
  TLS *t = TLS_DE(c);
  if (t->de_memoria) {
    if (usos) *usos = BR_KEYTYPE_KEYX | BR_KEYTYPE_SIGN;
    return br_x509_decoder_get_pkey(&t->dc);
  }
  return t->xc.vtable->get_pkey(&t->xc.vtable, usos);
}
static const br_x509_class xm_vtable = {
  sizeof(TLS), xm_start_chain, xm_start_cert, xm_append, xm_end_cert, xm_end_chain, xm_get_pkey
};

/* ---- las sesiones, para reanudarlas si el servidor quiere ---- */
static struct { char servidor[64]; br_ssl_session_parameters sp; } sesiones[4];
static int ses_sig;

TLS *WINAPI TlsAbrir(u32 s, const char *servidor) {
  static const uint16_t suites[] = {
    BR_TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256,
    BR_TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256,
    BR_TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
    BR_TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
    BR_TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
    BR_TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384
  };
  TLS *t;
  ST st;
  u8 az[32];
  u32 uno = 1, dias, n = strlen(servidor);
  int k, reanuda = 0, a, m;
  if (n > 63) return NULL;
  if (!pozo_listo) pozo_inicia();
  if (!n_anclas) anclas_inicia();
  t = malloc(sizeof *t);
  if (!t) return NULL;
  memset(t, 0, sizeof *t - sizeof t->entrada - sizeof t->salida - sizeof t->cadena);
  t->s = s;
  memcpy(t->servidor, servidor, n + 1);
  ioctlsocket(s, 0x8004667Eu, &uno);           /* sin bloqueo */

  br_ssl_client_zero(&t->cc);
  br_ssl_engine_set_versions(&t->cc.eng, BR_TLS12, BR_TLS12);
  br_ssl_engine_set_suites(&t->cc.eng, suites, sizeof suites / sizeof suites[0]);
  br_ssl_engine_set_default_rsavrfy(&t->cc.eng);
  br_ssl_engine_set_default_ecdsa(&t->cc.eng);
  br_ssl_engine_set_ec(&t->cc.eng, &br_ec_all_m31);
  br_ssl_engine_set_hash(&t->cc.eng, br_sha256_ID, &br_sha256_vtable);
  br_ssl_engine_set_hash(&t->cc.eng, br_sha384_ID, &br_sha384_vtable);
  br_ssl_engine_set_prf_sha256(&t->cc.eng, &br_tls12_sha256_prf);
  br_ssl_engine_set_prf_sha384(&t->cc.eng, &br_tls12_sha384_prf);
  br_ssl_engine_set_chacha20(&t->cc.eng, &br_chacha20_ct_run);
  br_ssl_engine_set_poly1305(&t->cc.eng, &br_poly1305_ctmul32_run);
  br_ssl_engine_set_chapol(&t->cc.eng, &br_sslrec_in_chapol_vtable, &br_sslrec_out_chapol_vtable);
  br_ssl_engine_set_aes_ctr(&t->cc.eng, &br_aes_big_ctr_vtable);
  br_ssl_engine_set_ghash(&t->cc.eng, &br_ghash_ctmul32);
  br_ssl_engine_set_gcm(&t->cc.eng, &br_sslrec_in_gcm_vtable, &br_sslrec_out_gcm_vtable);

  br_x509_minimal_init(&t->xc, &br_sha256_vtable, anclas, n_anclas);
  br_x509_minimal_set_rsa(&t->xc, &br_rsa_i31_pkcs1_vrfy);
  br_x509_minimal_set_ecdsa(&t->xc, &br_ec_all_m31, &br_ecdsa_i31_vrfy_asn1);
  br_x509_minimal_set_hash(&t->xc, br_sha256_ID, &br_sha256_vtable);
  br_x509_minimal_set_hash(&t->xc, br_sha384_ID, &br_sha384_vtable);
  br_x509_minimal_set_hash(&t->xc, br_sha512_ID, &br_sha512_vtable);
  /* la fecha del equipo (la hora local se toma como la universal: de sobra
   * para las fechas de un certificado): días desde el 1 de enero del año 0 */
  GetLocalTime(&st);
  a = st.anio; m = st.mes;
  dias = 365u * a + (a + 3) / 4 - (a + 99) / 100 + (a + 399) / 400;
  for (k = 1; k < m; k++)
    dias += (k == 2) ? (((a % 4 == 0 && a % 100 != 0) || a % 400 == 0) ? 29 : 28) : ((k == 4 || k == 6 || k == 9 || k == 11) ? 30 : 31);
  dias += st.dia - 1;
  br_x509_minimal_set_time(&t->xc, dias, st.hora * 3600u + st.min * 60u + st.seg);
  t->dias = dias;
  t->xv = &xm_vtable;
  br_ssl_engine_set_x509(&t->cc.eng, &t->xv);

  br_ssl_engine_set_buffers_bidi(&t->cc.eng, t->entrada, sizeof t->entrada, t->salida, sizeof t->salida);
  ruido();
  br_sha256_out(&pozo, az);
  br_sha256_update(&pozo, az, 8);
  br_ssl_engine_inject_entropy(&t->cc.eng, az, sizeof az);
  for (k = 0; k < 4; k++)
    if (!strcmp(sesiones[k].servidor, servidor)) {
      br_ssl_engine_set_session_parameters(&t->cc.eng, &sesiones[k].sp);
      reanuda = 1;
    }
  br_ssl_client_reset(&t->cc, t->servidor, reanuda);
  return t;
}

static u32 tsc(void) {
  u32 a = 0, d;
  if (hay_tsc) __asm__ volatile("rdtsc" : "=a"(a), "=d"(d));
  return a;
}
/* pasa registros entre BearSSL y el socket mientras se pueda (y cuenta los
 * ciclos que se van en BearSSL: todo menos send y recv) */
static void bombea2(TLS *t, u32 *c0);
static void bombea(TLS *t) {
  u32 c0 = tsc();
  bombea2(t, &c0);
  t->ciclos += tsc() - c0;
}
static void bombea2(TLS *t, u32 *c0) {
  br_ssl_engine_context *e = &t->cc.eng;
  for (;;) {
    unsigned st = br_ssl_engine_current_state(e);
    size_t n;
    int k, algo = 0;
    u8 *b;
    if (st & BR_SSL_CLOSED) {
      if (!t->error) t->error = br_ssl_engine_last_error(e);
      return;
    }
    if (st & BR_SSL_SENDREC) {
      b = br_ssl_engine_sendrec_buf(e, &n);
      t->ciclos += tsc() - *c0;
      k = send(t->s, (const char *)b, (int)n, 0);
      *c0 = tsc();
      if (k > 0) { br_ssl_engine_sendrec_ack(e, k); algo = 1; }
      else if (k < 0 && WSAGetLastError() != 10035) { t->fin_tcp = 1; if (!t->error) t->error = -WSAGetLastError(); return; }
    }
    if ((st & BR_SSL_RECVREC) && !t->fin_tcp) {
      b = br_ssl_engine_recvrec_buf(e, &n);
      t->ciclos += tsc() - *c0;
      k = recv(t->s, (char *)b, (int)n, 0);
      *c0 = tsc();
      if (k > 0) { ruido(); br_ssl_engine_recvrec_ack(e, k); algo = 1; }
      else if (k == 0) t->fin_tcp = 1;         /* cerró sin avisar (o tras avisar) */
      else if (WSAGetLastError() != 10035) { t->fin_tcp = 1; if (!t->error) t->error = -WSAGetLastError(); return; }
    }
    if (!algo) return;
  }
}
static int vencido(u32 t0, int ms) {
  if (ms < 0) return 0;
  return GetTickCount() - t0 >= (u32)ms;
}
int WINAPI TlsConectar(TLS *t, int ms) {
  u32 t0 = GetTickCount();
  for (;;) {
    unsigned st;
    bombea(t);
    st = br_ssl_engine_current_state(&t->cc.eng);
    if (st & BR_SSL_CLOSED) return -1;
    if (st & (BR_SSL_SENDAPP | BR_SSL_RECVAPP)) {
      if (!t->hecho) {                         /* se guarda la sesión */
        int k;
        t->hecho = 1;
        for (k = 0; k < 4 && strcmp(sesiones[k].servidor, t->servidor); k++) ;
        if (k == 4) k = ses_sig = (ses_sig + 1) & 3;
        memcpy(sesiones[k].servidor, t->servidor, sizeof t->servidor);
        br_ssl_engine_get_session_parameters(&t->cc.eng, &sesiones[k].sp);
      }
      return 1;
    }
    if (t->fin_tcp) { if (!t->error) t->error = -10054; return -1; }
    if (vencido(t0, ms)) return TLS_ESPERA;
    Sleep(0);
  }
}
int WINAPI TlsLeer(TLS *t, void *b, int n, int ms) {
  u32 t0 = GetTickCount();
  br_ssl_engine_context *e = &t->cc.eng;
  for (;;) {
    unsigned st;
    size_t q;
    u8 *p;
    bombea(t);
    st = br_ssl_engine_current_state(e);
    if (st & BR_SSL_RECVAPP) {
      p = br_ssl_engine_recvapp_buf(e, &q);
      if (q > (size_t)n) q = n;
      memcpy(b, p, q);
      br_ssl_engine_recvapp_ack(e, q);
      return (int)q;
    }
    if (st & BR_SSL_CLOSED) return (t->error == 0 || t->error == BR_ERR_OK) ? 0 : -1;
    if (t->fin_tcp) return 0;                  /* cortó sin el aviso de cierre */
    if (vencido(t0, ms)) return TLS_ESPERA;
    Sleep(0);
  }
}
int WINAPI TlsEscribir(TLS *t, const void *d, int n) {
  br_ssl_engine_context *e = &t->cc.eng;
  int hecho = 0;
  while (hecho < n) {
    unsigned st;
    size_t q;
    u8 *p;
    bombea(t);
    st = br_ssl_engine_current_state(e);
    if (st & BR_SSL_CLOSED) return -1;
    if (st & BR_SSL_SENDAPP) {
      p = br_ssl_engine_sendapp_buf(e, &q);
      if (q > (size_t)(n - hecho)) q = n - hecho;
      memcpy(p, (const u8 *)d + hecho, q);
      br_ssl_engine_sendapp_ack(e, q);
      hecho += (int)q;
      continue;
    }
    if (t->fin_tcp) { if (!t->error) t->error = -10054; return -1; }
    Sleep(0);
  }
  br_ssl_engine_flush(e, 0);
  bombea(t);
  return n;
}
void WINAPI TlsCerrar(TLS *t) {
  u32 t0;
  if (!t) return;
  if (t->hecho && !t->fin_tcp && !(br_ssl_engine_current_state(&t->cc.eng) & BR_SSL_CLOSED)) {
    br_ssl_engine_close(&t->cc.eng);           /* el aviso de cierre, si sale pronto */
    for (t0 = GetTickCount(); !vencido(t0, 300); Sleep(0)) {
      bombea(t);
      if (!(br_ssl_engine_current_state(&t->cc.eng) & BR_SSL_SENDREC)) break;
    }
  }
  free(t);
}
int WINAPI TlsError(TLS *t) { return t ? t->error : 0; }
u32 WINAPI TlsCiclos(TLS *t) { return t ? (u32)(t->ciclos >> 10) : 0; }
const char *WINAPI TlsTextoError(int e) {
  switch (e) {
  case 0: return "Sin error.";
  case BR_ERR_X509_EXPIRED:
    return "El certificado del servidor ha caducado o a\372n no vale. Compruebe la fecha del equipo.";
  case BR_ERR_X509_NOT_TRUSTED:
    return "El certificado del servidor no lo firma ninguna autoridad conocida.";
  case BR_ERR_X509_BAD_SERVER_NAME:
    return "El certificado es de otro sitio, no del que se ha pedido.";
  case BR_ERR_X509_BAD_SIGNATURE:
    return "La firma del certificado del servidor no es buena.";
  case BR_ERR_X509_WEAK_PUBLIC_KEY: case BR_ERR_X509_UNSUPPORTED: case BR_ERR_X509_WRONG_KEY_TYPE:
    return "El certificado del servidor usa una clave que aqu\355 no se admite.";
  case BR_ERR_BAD_VERSION: case BR_ERR_UNSUPPORTED_VERSION:
    return "El servidor no admite TLS 1.2.";
  case BR_ERR_BAD_CIPHER_SUITE: case BR_ERR_RECV_FATAL_ALERT + 40:
    return "El servidor no admite ninguno de los cifrados que hay aqu\355.";
  case BR_ERR_BAD_MAC:
    return "Han llegado datos cifrados que no cuadran.";
  case BR_ERR_NO_RANDOM:
    return "No hay azar suficiente para cifrar.";
  }
  if (e < 0) return "Se ha cortado la conexi\363n.";
  if (e >= BR_ERR_X509_OK && e < 64) return "El certificado del servidor no es v\341lido.";
  if (e >= BR_ERR_RECV_FATAL_ALERT) return "El servidor ha rechazado la conexi\363n segura.";
  return "Ha fallado la conexi\363n segura.";
}
/* «TLS 1.2, ECDHE-ECDSA, ChaCha20-Poly1305» (página 1252) */
int WINAPI TlsInfo(TLS *t, char *b, int n) {
  br_ssl_session_parameters sp;
  const char *c, *f;
  char r[80];
  u32 k = 0, l;
  if (!t || !t->hecho || n < 1) return 0;
  br_ssl_engine_get_session_parameters(&t->cc.eng, &sp);
  switch (sp.cipher_suite) {
  case BR_TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256: f = "ECDHE-ECDSA"; c = "ChaCha20-Poly1305"; break;
  case BR_TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256: f = "ECDHE-RSA"; c = "ChaCha20-Poly1305"; break;
  case BR_TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256: f = "ECDHE-ECDSA"; c = "AES-128-GCM"; break;
  case BR_TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256: f = "ECDHE-RSA"; c = "AES-128-GCM"; break;
  case BR_TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384: f = "ECDHE-ECDSA"; c = "AES-256-GCM"; break;
  default: f = "ECDHE-RSA"; c = "AES-256-GCM"; break;
  }
  memcpy(r, "TLS 1.2, ", 9); k = 9;
  l = strlen(f); memcpy(r + k, f, l); k += l;
  r[k++] = ','; r[k++] = ' ';
  l = strlen(c); memcpy(r + k, c, l + 1); k += l;
  if (k >= (u32)n) k = n - 1;
  memcpy(b, r, k); b[k] = 0;
  return (int)k;
}
/* BearSSL pregunta por el generador de azar del sistema: no hay (se le da
 * el azar con br_ssl_engine_inject_entropy, arriba) */
br_prng_seeder br_prng_seeder_system(const char **nombre) {
  if (nombre) *nombre = "ninguno";
  return 0;
}
