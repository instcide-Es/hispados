/* JDLL.DLL - la DLL de la prueba JUEGOV (JUEGOS): la que carga un juego
 * por su ruta relativa a la carpeta del .EXE («SUB\JDLL.DLL»).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
int Suma(int a, int b) { return a + b; }
int __attribute__((stdcall)) DllMain(void *h, unsigned r, void *x) { (void)h; (void)r; (void)x; return 1; }
