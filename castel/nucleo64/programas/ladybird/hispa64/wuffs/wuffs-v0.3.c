/* Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * nucleo64/programas/ladybird/hispa64/wuffs/wuffs-v0.3.c - Hispa-64, etapa 10 f
 * (docs/64/HISPA-64.md, sección 23): la capa de Hispa-64 para Ladybird. Su
 * GIFLoader.cpp incluye <wuffs/wuffs-v0.3.c>, que es solo Apache-2.0; en
 * Hispa-64 va la 0.4 («Apache-2.0 OR MIT», terceros/wuffs: se elige la MIT),
 * que tiene las mismas funciones del GIF (wuffs_gif__decoder...). Sin cambiar
 * nada de Ladybird: este archivo se pone delante en la ruta de cabeceras. */
#include <wuffs-v0.4.c>

/* Lo único que cambia para el GIF: la 0.3 tenía set_quirk_enabled (de sí o
 * no); la 0.4, set_quirk (con un valor: 1 es sí). */
static inline wuffs_base__empty_struct wuffs_gif__decoder__set_quirk_enabled(wuffs_gif__decoder* d, uint32_t q, bool v)
{
	wuffs_gif__decoder__set_quirk(d, q, v ? 1 : 0);
	return wuffs_base__make_empty_struct();
}
