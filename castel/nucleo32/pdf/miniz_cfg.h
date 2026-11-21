/* miniz_cfg.h - Hispa-PDF: miniz (terceros/miniz, MIT) sin lo que no hace
 * falta (los ZIP, stdio, la hora).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef HPDF_MINIZ_CFG_H
#define HPDF_MINIZ_CFG_H
#define MINIZ_NO_STDIO
#define MINIZ_NO_ARCHIVE_APIS
#define MINIZ_NO_TIME
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#ifndef NDEBUG
#define NDEBUG
#endif
#include "miniz.h"
#endif
