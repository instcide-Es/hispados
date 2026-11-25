/* cors.h - las reglas del origen de los dos Navegadores de Hispa-486
 * (w32/navega y w32/surf): el origen de una dirección («https://banco.es»,
 * el de localStorage y el de CORS), si dos son del mismo sitio (SameSite),
 * qué cabeceras de la página pueden ir en un fetch o un XMLHttpRequest y si
 * el servidor deja que la página lea la respuesta (Access-Control-Allow-*).
 * Sin nada de la red ni de JavaScript, para probarlo en el anfitrión con gcc
 * (tests/run_navegador_cors_tests.sh). docs/486/NAVEGADOR.md, «Lo que puede
 * pedir una página». Lo decide siempre C, con la dirección que sabe C (la de
 * la página), nunca con lo que diga el JavaScript de la página.
 * Se incluye (funciones static) en navega.c, script.c, red_http.c y guion.c.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef CORS_H
#define CORS_H

#define CORS_F __attribute__((unused)) static

CORS_F int cors_min(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }
/* ¿empiezan igual (n letras, sin mirar mayúsculas)? */
CORS_F int cors_igual(const char *a, const char *b, int n) {
  int k;
  for (k = 0; k < n; k++) if (cors_min((unsigned char)a[k]) != cors_min((unsigned char)b[k]) || !a[k]) return 0;
  return 1;
}
CORS_F int cors_largo(const char *t) { int n = 0; while (t[n]) n++; return n; }

/* el origen de u en d (cabe max): «http://servidor» o «https://servidor:puerto»
 * (el puerto, solo si no es el de siempre), en minúsculas. 0 si no tiene
 * (about:, file:, data:...: es «opaco» y no es de nadie). El servidor, hasta
 * '/', ':', '?' o '#', como url_parte (http.c), para que sea el mismo al que
 * se conecta */
CORS_F int cors_origen(const char *u, char *d, int max) {
  const char *p, *f;
  int seguro, puerto = 0, n = 0, k, defecto;
  char num[8];
  if (!u || max < 40) return 0;
  if (cors_igual(u, "https://", 8)) { seguro = 1; p = u + 8; }
  else if (cors_igual(u, "http://", 7)) { seguro = 0; p = u + 7; }
  else return 0;
  for (f = p; *f && *f != '/' && *f != ':' && *f != '?' && *f != '#' && *f != '\\'; f++) ;
  if (f == p || f - p > 127) return 0;
  if (*f == ':') {
    const char *q;
    for (q = f + 1; *q >= '0' && *q <= '9'; q++) { puerto = puerto * 10 + (*q - '0'); if (puerto > 65535) return 0; }
    if (*q && *q != '/' && *q != '?' && *q != '#') return 0;
  }
  defecto = seguro ? 443 : 80;
  if (!puerto) puerto = defecto;
  if ((f - p) + 8 + 7 >= max) return 0;
  if (seguro) { const char *e = "https://"; while (*e) d[n++] = *e++; }
  else { const char *e = "http://"; while (*e) d[n++] = *e++; }
  for (; p < f; p++) {
    int c = cors_min((unsigned char)*p);
    if (c <= ' ' || c == 127) return 0;       /* nada de espacios ni de control */
    d[n++] = (char)c;
  }
  if (puerto != defecto) {
    k = 0;
    while (puerto) { num[k++] = (char)('0' + puerto % 10); puerto /= 10; }
    d[n++] = ':';
    while (k) d[n++] = num[--k];
  }
  d[n] = 0;
  return n;
}
/* ¿son del mismo origen? (los dos tienen que tenerlo) */
CORS_F int cors_mismo(const char *a, const char *b) {
  char oa[160], ob[160];
  int k;
  if (!cors_origen(a, oa, sizeof oa) || !cors_origen(b, ob, sizeof ob)) return 0;
  for (k = 0; oa[k] == ob[k]; k++) if (!oa[k]) return 1;
  return 0;
}

/* ¿son de otro sitio? (SameSite: lo mismo de los dos últimos trozos del
 * nombre del servidor, «clicgest.es»). Sin saber de dónde se pide (la barra
 * de la dirección, un marcador), no lo es */
CORS_F const char *cors_sitio(const char *u, int *n) {
  const char *a, *f, *p1 = NULL, *p2 = NULL, *q;
  *n = 0;
  for (a = u; *a && !(a[0] == ':' && a[1] == '/' && a[2] == '/'); a++) ;
  if (!*a) return NULL;
  a += 3;
  for (f = a; *f && *f != '/' && *f != ':' && *f != '?' && *f != '#'; f++) ;
  for (q = a; q < f; q++) if (*q == '.') { p2 = p1; p1 = q; }
  if (p2) a = p2 + 1;
  *n = f - a;
  return a;
}
CORS_F int cors_cruzada(const char *de, const char *a) {
  int n1, n2;
  const char *s1, *s2;
  if (!de || !cors_igual(de, "http", 4)) return 0;
  if (!a) return 1;
  s1 = cors_sitio(de, &n1); s2 = cors_sitio(a, &n2);
  return !s1 || !s2 || n1 != n2 || !cors_igual(s1, s2, n1);
}

/* ---- lo que pide la página (fetch y XMLHttpRequest) ---- */
/* ¿es un «token» de HTTP (un nombre de cabecera o de método)? */
CORS_F int cors_token(const char *t, int n) {
  int k;
  if (n <= 0) return 0;
  for (k = 0; k < n; k++) {
    unsigned char c = (unsigned char)t[k];
    if (c <= ' ' || c >= 127 || c == ':' || c == '(' || c == ')' || c == ',' || c == '/' || c == ';' || c == '<' || c == '='
        || c == '>' || c == '?' || c == '@' || c == '[' || c == '\\' || c == ']' || c == '{' || c == '}' || c == '"') return 0;
  }
  return 1;
}
/* las que pone el Navegador y nunca la página (las «prohibidas» de Fetch,
 * más User-Agent y Accept-Language, que ya van siempre) */
CORS_F int cors_prohibida(const char *t, int n) {
  static const char *const no[] = { "accept-charset", "accept-encoding", "access-control-request-headers",
    "access-control-request-method", "connection", "content-length", "cookie", "cookie2", "date", "dnt", "expect",
    "host", "keep-alive", "origin", "referer", "set-cookie", "te", "trailer", "transfer-encoding", "upgrade", "via",
    "user-agent", "accept-language", NULL };
  int k;
  if ((n >= 6 && cors_igual(t, "proxy-", 6)) || (n >= 4 && cors_igual(t, "sec-", 4))) return 1;
  for (k = 0; no[k]; k++) if (cors_largo(no[k]) == n && cors_igual(t, no[k], n)) return 1;
  return 0;
}
/* ¿es «simple» (la deja CORS sin preguntar antes al servidor)? */
CORS_F int cors_simple(const char *t, int n, const char *v, int nv) {
  int k;
  if (nv > 128) return 0;
  if ((n == 6 && cors_igual(t, "accept", 6)) || (n == 16 && cors_igual(t, "content-language", 16))) return 1;
  if (n == 12 && cors_igual(t, "content-type", 12)) {
    static const char *const tipos[] = { "application/x-www-form-urlencoded", "multipart/form-data", "text/plain", NULL };
    while (nv && (v[nv - 1] == ' ' || v[nv - 1] == '\t')) nv--;
    for (k = 0; tipos[k]; k++) {
      int l = cors_largo(tipos[k]), j = l;
      if (nv < l || !cors_igual(v, tipos[k], l)) continue;
      while (j < nv && (v[j] == ' ' || v[j] == '\t')) j++;
      if (j == nv || v[j] == ';') return 1;
    }
  }
  return 0;
}
/* ¿se puede pedir? método: el de la página; cab: sus cabeceras
 * («Nombre: valor\r\n»...); otro: es a otro origen; origen: el de la página
 * (o NULL: opaco, «null»). En *d (malloc) las que se mandan: sin las
 * prohibidas ni las mal hechas (se quitan sin decir nada, como hacen los
 * navegadores) y, si es a otro origen, con «Origin: ...». Devuelve 0, o un
 * texto con por qué no (un método que no vale; a otro origen, uno que no sea
 * GET, HEAD o POST o una cabecera que no sea «simple»: el Navegador no
 * pregunta antes con OPTIONS, así que no las manda) */
CORS_F const char *cors_pide(const char *metodo, const char *cab, int otro, const char *origen, char **d) {
  int nm = metodo ? cors_largo(metodo) : 0, cap, n = 0;
  const char *l = cab ? cab : "";
  char *r;
  *d = NULL;
  if (!cors_token(metodo, nm)) return "el m\351todo no vale";
  if ((nm == 7 && cors_igual(metodo, "connect", 7)) || (nm == 5 && (cors_igual(metodo, "trace", 5) || cors_igual(metodo, "track", 5))))
    return "ese m\351todo no se deja";
  if (otro && !((nm == 3 && cors_igual(metodo, "GET", 3)) || (nm == 4 && (cors_igual(metodo, "HEAD", 4) || cors_igual(metodo, "POST", 4)))))
    return "a otro origen solo se piden GET, HEAD y POST (sin preguntar antes con OPTIONS)";
  cap = cors_largo(l) + (origen ? cors_largo(origen) : 4) + 16;
  if (!(r = malloc(cap))) return "no hay memoria";
  while (*l) {
    const char *e = l, *dos, *v;
    int nn, nv, k, malo = 0;
    while (*e && *e != '\n') e++;
    nv = e - l;
    if (nv && l[nv - 1] == '\r') nv--;
    for (dos = l; dos < l + nv && *dos != ':'; dos++) ;
    nn = dos - l;
    if (dos < l + nv && cors_token(l, nn)) {
      for (v = dos + 1; v < l + nv && (*v == ' ' || *v == '\t'); v++) ;
      nv = l + nv - v;
      for (k = 0; k < nv; k++) if (((unsigned char)v[k] < ' ' && v[k] != '\t') || v[k] == 127) malo = 1;
      if (!malo && !cors_prohibida(l, nn)) {
        if (otro && !cors_simple(l, nn, v, nv)) { free(r); return "a otro origen solo van las cabeceras simples (Accept, Content-Language y Content-Type de formulario o de texto)"; }
        memcpy(r + n, l, nn); n += nn;
        r[n++] = ':'; r[n++] = ' ';
        memcpy(r + n, v, nv); n += nv;
        r[n++] = '\r'; r[n++] = '\n';
      }
    }
    l = *e ? e + 1 : e;
  }
  if (otro) {
    const char *o = origen ? origen : "null";
    memcpy(r + n, "Origin: ", 8); n += 8;
    memcpy(r + n, o, cors_largo(o)); n += cors_largo(o);
    r[n++] = '\r'; r[n++] = '\n';
  }
  r[n] = 0;
  if (n > 2000) { free(r); return "demasiadas cabeceras"; }
  *d = r;
  return 0;
}

/* ---- lo que responde el servidor ---- */
/* el valor de la cabecera nombre en cab (las de la respuesta, «\nHTTP/1.1
 * 200 OK\r\nNombre: valor\r\n...»): en *v (sin los espacios de los lados) y
 * su largo; devuelve cuántas veces está */
CORS_F int cors_cabecera(const char *cab, const char *nombre, const char **v, int *nv) {
  int n = cors_largo(nombre), veces = 0;
  const char *l = cab;
  *v = NULL; *nv = 0;
  if (!cab) return 0;
  while ((l = strchr(l, '\n'))) {
    l++;
    if (cors_igual(l, nombre, n) && l[n] == ':') {
      const char *a = l + n + 1, *e;
      while (*a == ' ' || *a == '\t') a++;
      for (e = a; *e && *e != '\r' && *e != '\n'; e++) ;
      while (e > a && (e[-1] == ' ' || e[-1] == '\t')) e--;
      if (!veces) { *v = a; *nv = e - a; }
      veces++;
    }
  }
  return veces;
}
/* ¿deja el servidor que la página de origen lea la respuesta? galletas: han
 * ido galletas (credentials: 'include', withCredentials). Con
 * Access-Control-Allow-Origin: * (sin galletas) o el origen tal cual (con
 * galletas, además Access-Control-Allow-Credentials: true). Lo demás, no */
CORS_F int cors_deja(const char *cab, const char *origen, int galletas) {
  const char *v;
  int nv, no = origen ? cors_largo(origen) : 4;
  if (cors_cabecera(cab, "Access-Control-Allow-Origin", &v, &nv) != 1) return 0;
  if (nv == 1 && v[0] == '*') return !galletas;
  if (nv != no || memcmp(v, origen ? origen : "null", no)) return 0;
  if (!galletas) return 1;
  return cors_cabecera(cab, "Access-Control-Allow-Credentials", &v, &nv) == 1 && nv == 4 && !memcmp(v, "true", 4);
}
/* ¿está nombre (n) en la lista «a, b, c» (lv)? */
CORS_F int cors_en_lista(const char *lista, int lv, const char *nombre, int n) {
  int k = 0;
  while (k < lv) {
    int a, e;
    while (k < lv && (lista[k] == ' ' || lista[k] == '\t' || lista[k] == ',')) k++;
    a = k;
    while (k < lv && lista[k] != ',') k++;
    e = k;
    while (e > a && (lista[e - 1] == ' ' || lista[e - 1] == '\t')) e--;
    if (e - a == n && cors_igual(lista + a, nombre, n)) return 1;
  }
  return 0;
}
/* las cabeceras de la respuesta que ve la página (malloc): nunca Set-Cookie
 * (con las HttpOnly dentro); a otro origen, solo las «simples»
 * (Cache-Control, Content-Language, Content-Length, Content-Type, Expires,
 * Last-Modified, Pragma) y las de Access-Control-Expose-Headers. La línea
 * del estado, tal cual */
CORS_F char *cors_visibles(const char *cab, int otro, int galletas) {
  static const char *const simples[] = { "cache-control", "content-language", "content-length", "content-type",
                                         "expires", "last-modified", "pragma", NULL };
  const char *l = cab ? cab : "", *expo;
  int n = 0, nexpo, todas;
  char *r = malloc(cors_largo(l) + 1);
  if (!r) return NULL;
  cors_cabecera(l, "Access-Control-Expose-Headers", &expo, &nexpo);
  todas = !galletas && cors_en_lista(expo, nexpo, "*", 1);
  if (*l == '\n') r[n++] = *l++;
  {                                             /* la del estado */
    const char *e = l;
    while (*e && *e != '\n') e++;
    if (*e) e++;
    memcpy(r + n, l, e - l); n += e - l;
    l = e;
  }
  while (*l) {
    const char *e = l, *dos;
    int nn, k, vale;
    while (*e && *e != '\n') e++;
    if (*e) e++;
    for (dos = l; dos < e && *dos != ':' && *dos != '\r' && *dos != '\n'; dos++) ;
    nn = dos - l;
    vale = dos < e && *dos == ':';
    if (vale && ((nn == 10 && cors_igual(l, "set-cookie", 10)) || (nn == 11 && cors_igual(l, "set-cookie2", 11)))) vale = 0;
    if (vale && otro && !todas && !cors_en_lista(expo, nexpo, l, nn)) {
      vale = 0;
      for (k = 0; simples[k]; k++) if (cors_largo(simples[k]) == nn && cors_igual(l, simples[k], nn)) vale = 1;
    }
    if (!vale && !(dos < e && *dos == ':')) vale = 1;    /* la línea vacía del final */
    if (vale) { memcpy(r + n, l, e - l); n += e - l; }
    l = e;
  }
  r[n] = 0;
  return r;
}
#endif
