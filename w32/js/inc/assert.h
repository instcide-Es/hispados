/* Cabecera mínima para compilar QuickJS (terceros/quickjs) sin la biblioteca
 * del sistema, para HSPJS.DLL de Hispa-486 (w32/js, docs/486/NAVEGADOR.md).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef HJ_ASSERT_H
#define HJ_ASSERT_H
void hj_assert(const char *, const char *, int) __attribute__((noreturn));
#ifdef NDEBUG
#define assert(x) ((void)0)
#else
#define assert(x) ((x) ? (void)0 : hj_assert(#x, __FILE__, __LINE__))
#endif
#endif
