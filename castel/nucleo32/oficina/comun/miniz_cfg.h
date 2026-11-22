/* miniz_cfg.h - la oficina de 32 bits: miniz (terceros/miniz, MIT) con lo
 * de los ZIP (para ODT, DOCX, ODS, XLSX...), sin stdio ni la hora (la
 * memoria la da quien abre el ZIP: oficina/comun/zip.c).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef OFI_MINIZ_CFG_H
#define OFI_MINIZ_CFG_H
#define MINIZ_NO_STDIO
#define MINIZ_NO_TIME
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#ifndef NDEBUG
#define NDEBUG
#endif
#include "miniz.h"
#endif
