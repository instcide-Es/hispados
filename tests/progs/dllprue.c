/* DLLPRUE.DLL - una DLL de prueba para HW32: exporta una función (Doble) y un
 * dato (contador), y su DllMain suma 1 al contador al cargarse.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
int contador = 41;
static int interno(int x) { return x * 2; }
int __attribute__((stdcall)) Doble(int x) { return interno(x) + contador - 41; }
int __attribute__((stdcall)) DllMain(void *h, unsigned r, void *x) { if (r == 1) contador++; return 1; }
