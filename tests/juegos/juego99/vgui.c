/* VGUI.DLL del «juego de mentira» JUEGO99 (HL): como la de un juego de
 * 1999, va junto al .EXE, en la raíz del juego, y la importan las DLL de
 * las subcarpetas (MOD\CL_DLLS\CLIENTE.DLL), que Windows busca en la
 * carpeta del .EXE, no en la suya.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
int __attribute__((stdcall)) VguiPanel(int x) { return x * 2 + 1; }
int __attribute__((stdcall)) DllMain(void *h, unsigned r, void *x) { (void)h; (void)r; (void)x; return 1; }
