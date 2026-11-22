/* (temporal, para compilar el núcleo antes de tener los formatos y la ventana) */
#include <string.h>
#include "archivo.h"
#include "cuentas.h"
libro *hoj_lee(const u8 *d, u32 n, char *err, u32 cabe) { (void)d; (void)n; copia(err, "sin HOJ", cabe); return 0; }
int hoj_escribe(libro *l, bufer *b, char *err, u32 cabe) { (void)l; (void)b; copia(err, "sin HOJ", cabe); return 1; }
libro *csv_lee(const u8 *d, u32 n, const csv_opc *o, char *err, u32 cabe) { (void)d; (void)n; (void)o; copia(err, "x", cabe); return 0; }
int csv_escribe(libro *l, const csv_opc *o, bufer *b, char *err, u32 cabe) { (void)l; (void)o; (void)b; copia(err, "x", cabe); return 1; }
libro *xlsx_lee(const u8 *d, u32 n, char *err, u32 cabe) { (void)d; (void)n; copia(err, "x", cabe); return 0; }
int xlsx_escribe(libro *l, bufer *b, char *err, u32 cabe) { (void)l; (void)b; copia(err, "x", cabe); return 1; }
libro *ods_lee(const u8 *d, u32 n, char *err, u32 cabe) { (void)d; (void)n; copia(err, "x", cabe); return 0; }
int ods_escribe(libro *l, bufer *b, char *err, u32 cabe) { (void)l; (void)b; copia(err, "x", cabe); return 1; }
int pdf_exporta(libro *l, int h, const char *t, bufer *b, char *err, u32 cabe) { (void)l; (void)h; (void)t; (void)b; copia(err, "x", cabe); return 1; }
int cuentas_ventana(const char *a) { (void)a; return 1; }
int cuentas_mide(int n) { (void)n; return 1; }
#include "grafico.h"
void gr_nuevo(grafico *g, hoja *h, rango r, int tipo) { memset(g, 0, sizeof *g); g->tipo = (u8)tipo; g->hoja = h->id; g->datos = r; g->an = 480; g->al = 300; }
