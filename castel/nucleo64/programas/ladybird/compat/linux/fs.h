/* Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * nucleo64/programas/ladybird/compat/linux/fs.h - Hispa-64, etapa 10 e
 * (docs/64/HISPA-64.md, sección 22): lo de <linux/fs.h> (las cabeceras de
 * Linux, «UAPI», que el compilador cruzado aún no trae) que pide Ladybird
 * (LibFileSystem: renameat2 con RENAME_NOREPLACE). Los números, los de Linux. */
#pragma once
#ifndef RENAME_NOREPLACE
#define RENAME_NOREPLACE (1 << 0)
#define RENAME_EXCHANGE (1 << 1)
#define RENAME_WHITEOUT (1 << 2)
#endif
