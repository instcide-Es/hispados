/* Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * terceros/libpsl/hispa64/config.h - Hispa-64, etapa 10 e: lo que sacaría el
 * configure de libpsl 0.23.3 para x86_64-hispa64-linux-musl con la lista dentro
 * (--enable-builtin) y sin IDNA en marcha (--disable-runtime): Ladybird le pasa
 * los nombres ya en ASCII (LibURL los pasa a punycode antes). */
#define ENABLE_BUILTIN 1
#define HAVE_CLOCK_GETTIME 1
#define HAVE_DECL_LOCALTIME_R 1
#define HAVE_DIRENT_H 1
#define HAVE_FMEMOPEN 1
#define HAVE_INTTYPES_H 1
#define HAVE_NL_LANGINFO 1
#define HAVE_STDINT_H 1
#define HAVE_STDIO_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRINGS_H 1
#define HAVE_STRING_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_UNISTD_H 1
#define HAVE_VISIBILITY 1
#define PACKAGE_NAME "libpsl"
#define PACKAGE_VERSION "0.23.3"
#define PACKAGE_STRING "libpsl 0.23.3"
#define PACKAGE_TARNAME "libpsl"
#define PACKAGE_BUGREPORT "tim.ruehsen@gmx.de"
#define PACKAGE_URL ""
