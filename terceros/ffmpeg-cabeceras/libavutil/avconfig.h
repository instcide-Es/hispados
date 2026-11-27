/* Hispa-64, etapa 10 g: el avconfig.h que haría el configure de FFmpeg para
 * x86-64 (little endian, lecturas sin alinear rápidas). */
#ifndef AVUTIL_AVCONFIG_H
#define AVUTIL_AVCONFIG_H
#define AV_HAVE_BIGENDIAN 0
#define AV_HAVE_FAST_UNALIGNED 1
#endif /* AVUTIL_AVCONFIG_H */
