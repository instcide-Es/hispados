/* Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * hispa64/navegador/pdfpagina.h - Hispa-64, etapa 11 a (docs/64/HISPA-64.md,
 * sección 27): la página web (lo pintado por Ladybird, en PNG) a un PDF de
 * hojas A4 (pdfpagina.c). */
#ifndef NV_PDFPAGINA_H
#define NV_PDFPAGINA_H
#ifdef __cplusplus
extern "C" {
#endif

/* el PNG «png» a «pdf» (con el título, en UTF-8): 0 si va bien, con las
 * hojas en *hojas; si no, -1 y por qué en «error» */
int nv_png_a_pdf(const char *png, const char *pdf, const char *titulo, int *hojas, char *error, int cabe);

#ifdef __cplusplus
}
#endif
#endif
