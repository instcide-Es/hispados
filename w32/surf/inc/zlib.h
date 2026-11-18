/* Cabecera de la biblioteca de C para compilar NetSurf (terceros/netsurf) en
 * NAVEGA2.EXE (w32/surf): lo poco de zlib que usa NetSurf para leer sus
 * Messages; los archivos van sin comprimir (w32/surf/apoyo.c).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef HS_ZLIB_H
#define HS_ZLIB_H
typedef struct HS_FILE *gzFile;
gzFile gzopen(const char *, const char *);
char *gzgets(gzFile, char *, int);
int gzclose(gzFile);
int gzputs(gzFile, const char *);
/* (inflate: no hay; dice que no, y NetSurf lee sus textos del disco) */
typedef struct { const unsigned char *next_in; unsigned avail_in; unsigned long total_in;
  unsigned char *next_out; unsigned avail_out; unsigned long total_out; const char *msg; void *state;
  void *zalloc, *zfree, *opaque; int data_type; unsigned long adler, reserved; } z_stream;
#define Z_NULL 0
#define Z_OK 0
#define Z_STREAM_END 1
#define Z_DATA_ERROR (-3)
#define Z_NO_FLUSH 0
#define MAX_WBITS 15
int inflateInit2(z_stream *, int);
int inflate(z_stream *, int);
int inflateEnd(z_stream *);
#endif
