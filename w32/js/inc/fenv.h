/* Cabecera mínima para compilar QuickJS (terceros/quickjs) sin la biblioteca
 * del sistema, para HSPJS.DLL de Hispa-486 (w32/js, docs/486/NAVEGADOR.md).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef HJ_FENV_H
#define HJ_FENV_H
#define FE_TONEAREST 0
#define FE_DOWNWARD 0x400
#define FE_UPWARD 0x800
#define FE_TOWARDZERO 0xc00
int fesetround(int);
int fegetround(void);
#endif
