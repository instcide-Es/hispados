/* Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * nucleo64/programas/libs/mimg.c - Hispa-64, etapa 10 e (docs/64/HISPA-64.md,
 * sección 22): MIMG, la prueba de las bibliotecas de las imágenes que piden
 * Skia y Ladybird, compiladas con el compilador cruzado (hazimg.sh):
 *   - zlib 1.4: compress2 y uncompress, deflate con la cabecera de gzip e
 *     inflate que la reconoce sola, crc32 y adler32, y gzopen en /tmp;
 *   - libpng 1.6.59: un degradado RGBA de 64 x 48 a PNG en memoria y de
 *     vuelta (los mismos píxeles), guardado en /tmp; y PNGTEST.PNG (el de
 *     libpng) leído con la API de siempre (png_read_png);
 *   - libjpeg-turbo 3.2.1: el degradado a JPEG (calidad 85, de base y
 *     progresivo) y de vuelta, con su error, y a la mitad (scale_denom);
 *   - libwebp 1.6.0: sin pérdidas (los mismos píxeles), con pérdidas (su
 *     PSNR), una animación de 3 cuadros con WebPAnimEncoder, leída con
 *     WebPAnimDecoder (demux) y contada con WebPMux.
 * Lo que dice son cifras exactas (los tamaños y el crc32 de lo que sale):
 * las mismas en Linux y en Hispa-64. Lo que depende de las instrucciones
 * del procesador (lo de WebP con pérdidas) solo dice «bien» o «mal».
 * Al final, «MIMG: todo bien (N pruebas)» y código 0. */
#include <stdlib.h>
#include <unistd.h>
#include <setjmp.h>
#include <zlib.h>
#include <png.h>
#include <jpeglib.h>
#include <webp/decode.h>
#include <webp/encode.h>
#include <webp/demux.h>
#include <webp/mux.h>
#include "../musl/comun.h"

#define AN 64
#define AL 48

static unsigned char img[AN * AL * 4];   /* el degradado, RGBA */

static void haz_degradado(void)
{
	for (int y = 0; y < AL; y++)
		for (int x = 0; x < AN; x++) {
			unsigned char *p = img + (y * AN + x) * 4;
			p[0] = (unsigned char)(x * 4);
			p[1] = (unsigned char)(y * 5);
			p[2] = (unsigned char)((x + y) * 2);
			p[3] = (unsigned char)(255 - x);
		}
}

static unsigned long crc(const void *d, size_t n) { return crc32(0, d, (uInt)n); }

/* ---- zlib ---- */
static void prueba_zlib(void)
{
	static const char frase[] = "El pingüino Wenceslao hizo kilómetros bajo exhaustiva lluvia y frío, añoraba a su querido cañón. ";
	char texto[sizeof frase * 40];
	size_t n = 0;
	for (int i = 0; i < 40; i++) { memcpy(texto + n, frase, sizeof frase - 1); n += sizeof frase - 1; }
	unsigned char comp[8192], vuelta[8192];
	uLongf nc = sizeof comp, nv = sizeof vuelta;
	int r1 = compress2(comp, &nc, (const Bytef *)texto, n, 9);
	int r2 = uncompress(vuelta, &nv, comp, nc);
	MIRA(r1 == Z_OK && r2 == Z_OK && nv == n && memcmp(vuelta, texto, n) == 0, "zlib: compress2 y uncompress");
	MIRA(strcmp(zlibVersion(), ZLIB_VERSION) == 0, "zlib: la versión");
	/* con la cabecera de gzip (windowBits 31) e inflate que la ve sola (47) */
	z_stream z;
	unsigned char gz[8192];
	memset(&z, 0, sizeof z);
	deflateInit2(&z, 6, Z_DEFLATED, 31, 8, Z_DEFAULT_STRATEGY);
	z.next_in = (Bytef *)texto; z.avail_in = (uInt)n; z.next_out = gz; z.avail_out = sizeof gz;
	int r3 = deflate(&z, Z_FINISH);
	size_t ngz = z.total_out;
	deflateEnd(&z);
	memset(&z, 0, sizeof z);
	inflateInit2(&z, 47);
	z.next_in = gz; z.avail_in = (uInt)ngz; z.next_out = vuelta; z.avail_out = sizeof vuelta;
	int r4 = inflate(&z, Z_FINISH);
	size_t nv2 = z.total_out;
	inflateEnd(&z);
	MIRA(r3 == Z_STREAM_END && r4 == Z_STREAM_END && nv2 == n && memcmp(vuelta, texto, n) == 0 && gz[0] == 0x1f && gz[1] == 0x8b, "zlib: gzip en memoria");
	/* gzopen en /tmp */
	gzFile f = gzopen("/tmp/mimg.gz", "wb9");
	int w = f ? gzwrite(f, texto, (unsigned)n) : -1;
	if (f) gzclose(f);
	char leído[8192];
	f = gzopen("/tmp/mimg.gz", "rb");
	int l = f ? gzread(f, leído, sizeof leído) : -1;
	if (f) gzclose(f);
	unlink("/tmp/mimg.gz");
	MIRA(w == (int)n && l == (int)n && memcmp(leído, texto, n) == 0, "zlib: gzopen, gzwrite y gzread en /tmp");
	printf("  zlib %s: %zu octetos en %lu (nivel 9) y %zu con gzip; crc32 %08lx, adler32 %08lx; gzopen en /tmp\n",
	       zlibVersion(), n, (unsigned long)nc, ngz, crc(texto, n), (unsigned long)adler32(1, (const Bytef *)texto, (uInt)n));
}

/* ---- libpng ---- */
static size_t lee_archivo(const char *nombre, unsigned char **d)
{
	FILE *f = fopen(nombre, "rb");
	if (!f) return 0;
	fseek(f, 0, SEEK_END);
	long n = ftell(f);
	fseek(f, 0, SEEK_SET);
	*d = malloc(n > 0 ? (size_t)n : 1);
	size_t r = fread(*d, 1, (size_t)n, f);
	fclose(f);
	return r;
}

struct lector { const unsigned char *d; size_t n, pos; };
static void lee_mem(png_structp p, png_bytep b, png_size_t n)
{
	struct lector *l = png_get_io_ptr(p);
	if (l->pos + n > l->n) png_error(p, "se acaba");
	memcpy(b, l->d + l->pos, n);
	l->pos += n;
}

static void prueba_png(const char *carpeta)
{
	/* el degradado a PNG en memoria (la API sencilla) y de vuelta */
	png_image im;
	memset(&im, 0, sizeof im);
	im.version = PNG_IMAGE_VERSION; im.width = AN; im.height = AL; im.format = PNG_FORMAT_RGBA;
	png_alloc_size_t n = 0;
	int r1 = png_image_write_to_memory(&im, NULL, &n, 0, img, 0, NULL);
	unsigned char *png = malloc(n);
	int r2 = png_image_write_to_memory(&im, png, &n, 0, img, 0, NULL);
	png_image_free(&im);
	memset(&im, 0, sizeof im);
	im.version = PNG_IMAGE_VERSION;
	unsigned char *vuelta = NULL;
	int r3 = png_image_begin_read_from_memory(&im, png, n);
	if (r3) {
		im.format = PNG_FORMAT_RGBA;
		vuelta = malloc(PNG_IMAGE_SIZE(im));
		r3 = png_image_finish_read(&im, NULL, vuelta, 0, NULL);
	}
	MIRA(r1 && r2 && r3 && im.width == AN && im.height == AL && memcmp(vuelta, img, sizeof img) == 0, "libpng: el degradado, de ida y vuelta");
	FILE *f = fopen("/tmp/mimg.png", "wb");
	size_t esc = f ? fwrite(png, 1, n, f) : 0;
	if (f) fclose(f);
	unsigned char *otra = NULL;
	size_t nl = lee_archivo("/tmp/mimg.png", &otra);
	MIRA(esc == n && nl == n && memcmp(otra, png, n) == 0, "libpng: guardado en /tmp/mimg.png");
	printf("  libpng %s: el degradado de %d x %d (RGBA) en %lu octetos, crc32 %08lx, de ida y vuelta; guardado en /tmp/mimg.png\n",
	       png_get_libpng_ver(NULL), AN, AL, (unsigned long)n, crc(png, n));
	free(png); free(vuelta); free(otra);
	/* PNGTEST.PNG con la API de siempre */
	char ruta[512];
	snprintf(ruta, sizeof ruta, "%s/PNGTEST.PNG", carpeta);
	unsigned char *d = NULL;
	size_t nd = lee_archivo(ruta, &d);
	MIRA(nd > 0, "libpng: falta PNGTEST.PNG");
	if (!nd) return;
	struct lector l = { d, nd, 0 };
	png_structp p = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	png_infop i = png_create_info_struct(p);
	unsigned long c = 0;
	png_uint_32 an = 0, al = 0;
	int prof = 0, tipo = 0, entre = 0;
	if (setjmp(png_jmpbuf(p)) == 0) {
		png_set_read_fn(p, &l, lee_mem);
		png_read_png(p, i, PNG_TRANSFORM_EXPAND | PNG_TRANSFORM_STRIP_16 | PNG_TRANSFORM_GRAY_TO_RGB, NULL);
		png_get_IHDR(p, i, &an, &al, &prof, &tipo, &entre, NULL, NULL);
		png_bytepp filas = png_get_rows(p, i);
		size_t fb = png_get_rowbytes(p, i);
		for (png_uint_32 y = 0; y < al; y++) c = crc32(c, filas[y], (uInt)fb);
		MIRA(an == 91 && al == 69, "libpng: PNGTEST.PNG de 91 x 69");
	} else MIRA(0, "libpng: PNGTEST.PNG no se lee");
	png_destroy_read_struct(&p, &i, NULL);
	printf("  libpng: PNGTEST.PNG (%zu octetos) de %u x %u, %d canales; los píxeles, crc32 %08lx\n",
	       nd, (unsigned)an, (unsigned)al, tipo == PNG_COLOR_TYPE_RGB_ALPHA ? 4 : 3, c);
	free(d);
}

/* ---- libjpeg-turbo ---- */
struct error_jpeg { struct jpeg_error_mgr e; jmp_buf salto; };
static void sale_jpeg(j_common_ptr c) { longjmp(((struct error_jpeg *)c->err)->salto, 1); }

static unsigned char *a_jpeg(const unsigned char *rgb, int progresivo, unsigned long *n)
{
	struct jpeg_compress_struct c;
	struct error_jpeg e;
	unsigned char *sal = NULL;
	c.err = jpeg_std_error(&e.e);
	e.e.error_exit = sale_jpeg;
	if (setjmp(e.salto)) { jpeg_destroy_compress(&c); return NULL; }
	jpeg_create_compress(&c);
	jpeg_mem_dest(&c, &sal, n);
	c.image_width = AN; c.image_height = AL; c.input_components = 3; c.in_color_space = JCS_RGB;
	jpeg_set_defaults(&c);
	jpeg_set_quality(&c, 85, TRUE);
	if (progresivo) jpeg_simple_progression(&c);
	jpeg_start_compress(&c, TRUE);
	while (c.next_scanline < c.image_height) {
		JSAMPROW f = (JSAMPROW)(rgb + c.next_scanline * AN * 3);
		jpeg_write_scanlines(&c, &f, 1);
	}
	jpeg_finish_compress(&c);
	jpeg_destroy_compress(&c);
	return sal;
}

static unsigned char *de_jpeg(const unsigned char *d, unsigned long n, int denom, int *an, int *al)
{
	struct jpeg_decompress_struct c;
	struct error_jpeg e;
	unsigned char *sal = NULL;
	c.err = jpeg_std_error(&e.e);
	e.e.error_exit = sale_jpeg;
	if (setjmp(e.salto)) { jpeg_destroy_decompress(&c); free(sal); return NULL; }
	jpeg_create_decompress(&c);
	jpeg_mem_src(&c, d, n);
	jpeg_read_header(&c, TRUE);
	c.out_color_space = JCS_RGB;
	c.scale_num = 1; c.scale_denom = (unsigned)denom;
	jpeg_start_decompress(&c);
	*an = (int)c.output_width; *al = (int)c.output_height;
	sal = malloc((size_t)*an * *al * 3);
	while (c.output_scanline < c.output_height) {
		JSAMPROW f = sal + c.output_scanline * *an * 3;
		jpeg_read_scanlines(&c, &f, 1);
	}
	jpeg_finish_decompress(&c);
	jpeg_destroy_decompress(&c);
	return sal;
}

static void prueba_jpeg(void)
{
	unsigned char rgb[AN * AL * 3];
	for (int i = 0; i < AN * AL; i++) memcpy(rgb + i * 3, img + i * 4, 3);
	unsigned long n1 = 0, n2 = 0;
	unsigned char *j1 = a_jpeg(rgb, 0, &n1), *j2 = a_jpeg(rgb, 1, &n2);
	int an = 0, al = 0, an2 = 0, al2 = 0, máx = 0;
	unsigned char *v = j1 ? de_jpeg(j1, n1, 1, &an, &al) : NULL;
	unsigned char *vp = j2 ? de_jpeg(j2, n2, 1, &an2, &al2) : NULL;
	double se = 0;
	if (v && an == AN && al == AL)
		for (int i = 0; i < AN * AL * 3; i++) {
			int d = abs((int)v[i] - (int)rgb[i]);
			if (d > máx) máx = d;
			se += (double)d * d;
		}
	double psnr = se > 0 ? 10.0 * __builtin_log10(255.0 * 255.0 / (se / (AN * AL * 3))) : 99;
	MIRA(j1 && v && an == AN && al == AL && psnr > 35, "libjpeg: de ida y vuelta");
	MIRA(j2 && vp && an2 == AN && al2 == AL && n2 != n1, "libjpeg: progresivo");
	int an3 = 0, al3 = 0;
	unsigned char *m = j1 ? de_jpeg(j1, n1, 2, &an3, &al3) : NULL;
	MIRA(m && an3 == AN / 2 && al3 == AL / 2, "libjpeg: a la mitad");
	printf("  libjpeg-turbo %d (libjpeg %d): el degradado a JPEG de calidad 85 en %lu octetos (crc32 %08lx), progresivo en %lu (crc32 %08lx); de vuelta, %d de error como mucho (PSNR %.1f dB); a la mitad, %d x %d (crc32 %08lx)\n",
	       LIBJPEG_TURBO_VERSION_NUMBER, JPEG_LIB_VERSION, n1, j1 ? crc(j1, n1) : 0, n2, j2 ? crc(j2, n2) : 0, máx, psnr, an3, al3,
	       m ? crc(m, (size_t)an3 * al3 * 3) : 0);
	free(j1); free(j2); free(v); free(vp); free(m);
}

/* ---- libwebp ---- */
static void prueba_webp(void)
{
	uint8_t *sp = NULL, *cp = NULL;
	size_t ns = WebPEncodeLosslessRGBA(img, AN, AL, AN * 4, &sp);
	int an = 0, al = 0;
	uint8_t *v = ns ? WebPDecodeRGBA(sp, ns, &an, &al) : NULL;
	MIRA(v && an == AN && al == AL && memcmp(v, img, sizeof img) == 0, "libwebp: sin pérdidas, de ida y vuelta");
	WebPBitstreamFeatures f;
	MIRA(ns && WebPGetFeatures(sp, ns, &f) == VP8_STATUS_OK && f.has_alpha && f.format == 2, "libwebp: WebPGetFeatures");
	size_t nc = WebPEncodeRGBA(img, AN, AL, AN * 4, 75, &cp);
	uint8_t *vc = nc ? WebPDecodeRGBA(cp, nc, &an, &al) : NULL;
	double se = 0;
	if (vc) for (int i = 0; i < AN * AL * 4; i++) { double d = (double)vc[i] - img[i]; se += d * d; }
	double psnr = se > 0 ? 10.0 * __builtin_log10(255.0 * 255.0 / (se / (AN * AL * 4))) : 99;
	MIRA(vc && an == AN && al == AL && psnr > 30, "libwebp: con pérdidas (calidad 75)");
	/* una animación de 3 cuadros de 32 x 32, sin pérdidas */
	WebPAnimEncoderOptions eo;
	WebPAnimEncoderOptionsInit(&eo);
	WebPAnimEncoder *ae = WebPAnimEncoderNew(32, 32, &eo);
	WebPConfig cf;
	WebPConfigInit(&cf);
	cf.lossless = 1;
	uint32_t cuadros[3][32 * 32];
	for (int k = 0; k < 3; k++) {
		WebPPicture p;
		WebPPictureInit(&p);
		p.use_argb = 1; p.width = 32; p.height = 32;
		WebPPictureAlloc(&p);
		for (int i = 0; i < 32 * 32; i++) {
			int x = i % 32, y = i / 32;
			cuadros[k][i] = 0xFF000000u | (uint32_t)((x * 8 + k * 40) & 255) << 16 | (uint32_t)((y * 8) & 255) << 8 | (uint32_t)(k * 100);
			p.argb[i] = cuadros[k][i];
		}
		WebPAnimEncoderAdd(ae, &p, k * 100, &cf);
		WebPPictureFree(&p);
	}
	WebPAnimEncoderAdd(ae, NULL, 300, NULL);
	WebPData anim;
	WebPDataInit(&anim);
	int ok = WebPAnimEncoderAssemble(ae, &anim);
	WebPAnimEncoderDelete(ae);
	int bien_cuadros = 0, leídos = 0, último = 0;
	WebPAnimDecoderOptions dop;
	WebPAnimDecoderOptionsInit(&dop);
	dop.color_mode = MODE_BGRA;
	WebPAnimDecoder *ad = ok ? WebPAnimDecoderNew(&anim, &dop) : NULL;
	WebPAnimInfo ai;
	memset(&ai, 0, sizeof ai);
	if (ad) {
		WebPAnimDecoderGetInfo(ad, &ai);
		while (WebPAnimDecoderHasMoreFrames(ad)) {
			uint8_t *b; int t;
			if (!WebPAnimDecoderGetNext(ad, &b, &t)) break;
			if (leídos < 3 && memcmp(b, cuadros[leídos], sizeof cuadros[0]) == 0) bien_cuadros++;   /* BGRA en memoria = ARGB de 32 bits */
			leídos++;
			último = t;
		}
		WebPAnimDecoderDelete(ad);
	}
	MIRA(ok && ai.frame_count == 3 && leídos == 3 && bien_cuadros == 3 && último == 300, "libwebp: la animación (WebPAnimEncoder y WebPAnimDecoder)");
	WebPMux *mx = ok ? WebPMuxCreate(&anim, 0) : NULL;
	int nf = -1;
	if (mx) { WebPMuxNumChunks(mx, WEBP_CHUNK_ANMF, &nf); WebPMuxDelete(mx); }
	MIRA(nf == 3, "libwebp: WebPMux cuenta los cuadros");
	int ve = WebPGetDecoderVersion();
	printf("  libwebp %d.%d.%d: el degradado sin pérdidas en %zu octetos (crc32 %08lx), los mismos píxeles; con pérdidas (calidad 75), %s; una animación de %d cuadros en %zu octetos (crc32 %08lx), leída con demux y contada con mux\n",
	       ve >> 16, (ve >> 8) & 255, ve & 255, ns, ns ? crc(sp, ns) : 0, psnr > 30 ? "bien" : "mal", leídos, anim.size, crc(anim.bytes, anim.size));
	WebPDataClear(&anim);
	WebPFree(sp); WebPFree(cp); WebPFree(v); WebPFree(vc);
}

int main(int argc, char **argv)
{
	char carpeta[512] = ".";
	/* PNGTEST.PNG va con el programa: en su misma carpeta (/proc/self/exe) */
	ssize_t r = readlink("/proc/self/exe", carpeta, sizeof carpeta - 1);
	if (r > 0) { carpeta[r] = 0; char *b = strrchr(carpeta, '/'); if (b) *b = 0; }   /* («/MIMG»: «») */
	else strcpy(carpeta, ".");
	if (argc > 1) snprintf(carpeta, sizeof carpeta, "%s", argv[1]);
	printf("MIMG: las bibliotecas de las imágenes (zlib, libpng, libjpeg-turbo, libwebp); %s\n", cómo());
	fflush(stdout);
	haz_degradado();
	prueba_zlib();
	prueba_png(carpeta);
	prueba_jpeg();
	prueba_webp();
	return acaba("MIMG");
}
