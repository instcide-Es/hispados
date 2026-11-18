/* =============================================================================
 *  JS.EXE de Hispa-486: JavaScript desde la línea de órdenes, con HSPJS.DLL
 *  (QuickJS). Para probar el motor sin el Navegador.
 *  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 *
 *     JS [/M] [/T:segundos] archivo.js [argumentos]
 *     JS /E "código"
 *
 *  Si el archivo es un módulo (import/export), se carga como módulo, y sus
 *  import, de archivos junto a él. Hay print() y console.log, y lo que piden
 *  las pruebas de QuickJS: std.gc(), std.loadFile(), os.setTimeout(),
 *  os.platform y scriptArgs. /M dice al final la memoria y el tiempo. /T
 *  para el programa si pasa de ese tiempo. Sale con 1 si hay una excepción.
 * ============================================================================= */
#include "quickjs.h"

typedef unsigned int u32;
#define WINAPI __attribute__((stdcall))
typedef struct MFILE MFILE;
MFILE *fopen(const char *, const char *);
int fclose(MFILE *);
size_t fread(void *, size_t, size_t, MFILE *);
int fseek(MFILE *, long, int);
long ftell(MFILE *);
int printf(const char *, ...);
void *malloc(size_t);
void free(void *);
int _write(int, const void *, unsigned);
u32 WINAPI GetTickCount(void);
void HjMemoria(u32 *uso, u32 *maximo, u32 *sistema);

static int con_memoria, tope_s;
static u32 t0;

static char *lee_archivo(const char *nombre, size_t *n) {
  MFILE *f = fopen(nombre, "rb");
  long l;
  char *b;
  if (!f) return NULL;
  fseek(f, 0, 2);
  l = ftell(f);
  fseek(f, 0, 0);
  b = malloc(l + 1);
  if (b) { *n = fread(b, 1, l, f); b[*n] = 0; }
  fclose(f);
  return b;
}
static void escribe(const char *t, size_t n) { _write(1, t, (unsigned)n); }

/* print(...) y console.log(...): lo de cada argumento, separado por espacios */
static JSValue js_print(JSContext *c, JSValueConst este, int argc, JSValueConst *argv) {
  int k;
  (void)este;
  for (k = 0; k < argc; k++) {
    size_t n;
    const char *t = JS_ToCStringLen(c, &n, argv[k]);
    if (k) escribe(" ", 1);
    if (t) { escribe(t, n); JS_FreeCString(c, t); }
  }
  escribe("\r\n", 2);
  return JS_UNDEFINED;
}
static JSValue js_gc(JSContext *c, JSValueConst este, int argc, JSValueConst *argv) {
  (void)este; (void)argc; (void)argv;
  JS_RunGC(JS_GetRuntime(c));
  return JS_UNDEFINED;
}
static JSValue js_carga(JSContext *c, JSValueConst este, int argc, JSValueConst *argv) {
  const char *nom;
  char *b;
  size_t n = 0;
  JSValue r;
  (void)este; (void)argc;
  if (!(nom = JS_ToCString(c, argv[0]))) return JS_EXCEPTION;
  b = lee_archivo(nom, &n);
  JS_FreeCString(c, nom);
  if (!b) return JS_NULL;
  r = JS_NewStringLen(c, b, n);
  free(b);
  return r;
}
static JSValue js_carga_guion(JSContext *c, JSValueConst este, int argc, JSValueConst *argv) {
  const char *nom;
  char *b;
  size_t n = 0;
  JSValue r;
  (void)este; (void)argc;
  if (!(nom = JS_ToCString(c, argv[0]))) return JS_EXCEPTION;
  b = lee_archivo(nom, &n);
  if (!b) { r = JS_ThrowReferenceError(c, "no se puede leer %s", nom); JS_FreeCString(c, nom); return r; }
  r = JS_Eval(c, b, n, nom, JS_EVAL_TYPE_GLOBAL);
  JS_FreeCString(c, nom);
  free(b);
  return r;
}

/* os.setTimeout: una lista que se vacía al acabar el programa, por orden de
 * tiempo (no se espera de verdad) */
#define TEMP_MAX 64
static struct { JSValue f; double cuando; int vivo; } temp[TEMP_MAX];
static double reloj_falso;
static JSValue js_settimeout(JSContext *c, JSValueConst este, int argc, JSValueConst *argv) {
  int k;
  double ms = 0;
  (void)este;
  if (argc > 1) JS_ToFloat64(c, &ms, argv[1]);
  for (k = 0; k < TEMP_MAX && temp[k].vivo; k++) ;
  if (k == TEMP_MAX) return JS_ThrowInternalError(c, "demasiados temporizadores");
  temp[k].f = JS_DupValue(c, argv[0]);
  temp[k].cuando = reloj_falso + ms;
  temp[k].vivo = 1;
  return JS_NewInt32(c, k + 1);
}
static JSValue js_cleartimeout(JSContext *c, JSValueConst este, int argc, JSValueConst *argv) {
  int k = 0;
  (void)este; (void)argc;
  JS_ToInt32(c, &k, argv[0]);
  if (k >= 1 && k <= TEMP_MAX && temp[k - 1].vivo) { JS_FreeValue(c, temp[k - 1].f); temp[k - 1].vivo = 0; }
  return JS_UNDEFINED;
}

static void excepcion(JSContext *c) {
  JSValue e = JS_GetException(c), pila;
  const char *t = JS_ToCString(c, e);
  printf("Error: %s\r\n", t ? t : "(sin texto)");
  if (t) JS_FreeCString(c, t);
  if (JS_IsError(c, e)) {
    pila = JS_GetPropertyStr(c, e, "stack");
    if (!JS_IsUndefined(pila) && (t = JS_ToCString(c, pila))) { printf("%s\r\n", t); JS_FreeCString(c, t); }
    JS_FreeValue(c, pila);
  }
  JS_FreeValue(c, e);
}
static JSModuleDef *carga_modulo(JSContext *c, const char *nombre, void *x) {
  size_t n = 0;
  char *b = lee_archivo(nombre, &n);
  JSValue f;
  JSModuleDef *m;
  (void)x;
  if (!b) { JS_ThrowReferenceError(c, "no se encuentra el modulo %s", nombre); return NULL; }
  f = JS_Eval(c, b, n, nombre, JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
  free(b);
  if (JS_IsException(f)) return NULL;
  m = JS_VALUE_GET_PTR(f);
  JS_FreeValue(c, f);
  return m;
}
static int corta(JSRuntime *rt, void *x) {
  (void)rt; (void)x;
  return tope_s && GetTickCount() - t0 > (u32)tope_s * 1000;
}
static void funcion(JSContext *c, JSValue obj, const char *nom, JSCFunction *f, int n) {
  JS_SetPropertyStr(c, obj, nom, JS_NewCFunction(c, f, nom, n));
}

int main(int argc, char **argv) {
  JSRuntime *rt;
  JSContext *c;
  JSValue g, o, r, args;
  char *fuente;
  size_t n = 0;
  int k = 1, fallo = 0, j;
  const char *nombre;
  t0 = GetTickCount();
  for (; k < argc && argv[k][0] == '/'; k++) {
    if ((argv[k][1] | 32) == 'm') con_memoria = 1;
    else if ((argv[k][1] | 32) == 't' && argv[k][2] == ':') { const char *p = argv[k] + 3; tope_s = 0; while (*p >= '0' && *p <= '9') tope_s = tope_s * 10 + *p++ - '0'; }
    else if ((argv[k][1] | 32) == 'e') break;
  }
  if (k >= argc) {
    printf("JS de Hispa-486 (QuickJS " CONFIG_VERSION ")\r\nUso: JS [/M] [/T:segundos] archivo.js [argumentos]\r\n     JS /E \"codigo\"\r\n");
    return 2;
  }
  rt = JS_NewRuntime();
  JS_SetMaxStackSize(rt, 160 * 1024);
  JS_SetInterruptHandler(rt, corta, NULL);
  JS_SetModuleLoaderFunc(rt, NULL, carga_modulo, NULL);
  c = JS_NewContext(rt);
  g = JS_GetGlobalObject(c);
  funcion(c, g, "print", js_print, 1);
  funcion(c, g, "__loadScript", js_carga_guion, 1);
  o = JS_NewObject(c);
  funcion(c, o, "log", js_print, 1);
  funcion(c, o, "error", js_print, 1);
  JS_SetPropertyStr(c, g, "console", o);
  o = JS_NewObject(c);
  funcion(c, o, "gc", js_gc, 0);
  funcion(c, o, "loadFile", js_carga, 1);
  JS_SetPropertyStr(c, g, "std", o);
  o = JS_NewObject(c);
  funcion(c, o, "setTimeout", js_settimeout, 2);
  funcion(c, o, "clearTimeout", js_cleartimeout, 1);
  JS_SetPropertyStr(c, o, "platform", JS_NewString(c, "hispa"));
  JS_SetPropertyStr(c, g, "os", o);
  args = JS_NewArray(c);
  for (j = k; j < argc; j++) JS_SetPropertyUint32(c, args, j - k, JS_NewString(c, argv[j]));
  JS_SetPropertyStr(c, g, "scriptArgs", args);
  JS_FreeValue(c, g);

  if ((argv[k][1] | 32) == 'e' && argv[k][0] == '/') {
    if (k + 1 >= argc) return 2;
    nombre = "<orden>";
    fuente = argv[k + 1];
    n = strlen(fuente);
    r = JS_Eval(c, fuente, n, nombre, JS_EVAL_TYPE_GLOBAL);
    fuente = NULL;
  } else {
    nombre = argv[k];
    fuente = lee_archivo(nombre, &n);
    if (!fuente) { printf("No se puede leer %s\r\n", nombre); return 2; }
    if (JS_DetectModule(fuente, n)) {
      r = JS_Eval(c, fuente, n, nombre, JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
      if (!JS_IsException(r)) r = JS_EvalFunction(c, r);
    } else r = JS_Eval(c, fuente, n, nombre, JS_EVAL_TYPE_GLOBAL);
  }
  if (JS_IsException(r)) { excepcion(c); fallo = 1; }
  JS_FreeValue(c, r);
  /* las promesas y los temporizadores, hasta que no quede nada */
  for (;;) {
    JSContext *c2;
    int e, mejor = -1;
    while ((e = JS_ExecutePendingJob(rt, &c2)) != 0) if (e < 0) { excepcion(c2); fallo = 1; }
    for (j = 0; j < TEMP_MAX; j++) if (temp[j].vivo && (mejor < 0 || temp[j].cuando < temp[mejor].cuando)) mejor = j;
    if (mejor < 0) break;
    {
      JSValue f = temp[mejor].f;
      temp[mejor].vivo = 0;
      reloj_falso = temp[mejor].cuando;
      r = JS_Call(c, f, JS_UNDEFINED, 0, NULL);
      if (JS_IsException(r)) { excepcion(c); fallo = 1; }
      JS_FreeValue(c, r);
      JS_FreeValue(c, f);
    }
  }
  if (con_memoria) {
    u32 uso, max, sis;
    HjMemoria(&uso, &max, &sis);
    printf("Memoria: %u KB en uso, %u KB como mucho, %u KB pedidos al sistema; %u ms\r\n",
           (uso + 512) / 1024, (max + 512) / 1024, (sis + 512) / 1024, GetTickCount() - t0);
  }
  free(fuente);
  JS_FreeContext(c);
  JS_FreeRuntime(rt);
  return fallo;
}
