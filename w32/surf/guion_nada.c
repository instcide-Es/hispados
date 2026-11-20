/* guion_nada.c - [JS] para el Navegador completo en el anfitrión (surfppm,
 * w32/surf/anfitrion.c): sin JavaScript (pinta la página como llega). Lo que
 * piden los ganchos [HISPA] de NetSurf a guion.c, vacío; la interfaz de
 * javascript/js.h la pone none.c de NetSurf.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <stdbool.h>
#include <stddef.h>
#include "guion.h"
struct jsthread *guion_de(dom_node *n) { (void)n; return NULL; }
bool guion_evento(struct jsthread *t, dom_node *n, const char *tipo, dom_node *otro) { (void)t; (void)n; (void)tipo; (void)otro; return false; }
bool guion_tecla(struct jsthread *t, dom_node *n, const char *tipo, unsigned tecla) { (void)t; (void)n; (void)tipo; (void)tecla; return false; }
void guion_foco(struct jsthread *t, dom_node *n) { (void)t; (void)n; }
void guion_evento_luego(struct jsthread *t, dom_node *n, const char *tipo) { (void)t; (void)n; (void)tipo; }
void guion_foco_luego(struct jsthread *t, dom_node *n) { (void)t; (void)n; }
int guion_ocupado(void) { return 0; }
