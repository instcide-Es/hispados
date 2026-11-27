// Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
// nucleo64/programas/cxx/mskia.cpp - Hispa-64, etapa 10 e (docs/64/HISPA-64.md,
// sección 22): MSKIA, la prueba de Skia (m148, la de Ladybird) dibujando con
// el procesador (sin GPU) en un mapa de bits de 400 x 240:
//   - un degradado lineal (SkGradient), formas con suavizado (un círculo con
//     borde, un rectángulo redondeado, una estrella con curvas, una línea de
//     rayas: SkDashPathEffect), una sombra difuminada (SkMaskFilter), la
//     unión de dos caminos (SkPathOps) y un sombreador de SkSL
//     (SkRuntimeEffect: lo compila Skia en marcha);
//   - texto con eñes y acentos en DejaVu Sans (DEJAVU.TTF), colocado con
//     HarfBuzz (SkShaper, con SkUnicode sobre ICU: icudt77l.dat) y con
//     SkFont a secas;
//   - PNGTEST.PNG (el de libpng) leído con SkPngDecoder y dibujado a 1,5 veces
//     con filtro lineal;
//   - el resultado, a PNG (SkPngEncoder) en /tmp/mskia.png, leído otra vez
//     (los mismos píxeles); y a JPEG y WebP (SkJpegEncoder, SkWebpEncoder) y
//     de vuelta (SkJpegDecoder, SkWebpDecoder).
// Dice el crc32 de los píxeles y del PNG (los mismos en Linux y en Hispa-64
// si Skia va por el mismo camino: SSE2, o AVX2 con FMA —«ML3»—, o AVX-512
// —«ML4»—, que elige al arrancar según el procesador), y una «huella» (la
// media de cada cuadro de 16 x 16, en RGB) que debe ser casi la misma
// (como mucho 2 de diferencia) aunque el camino sea otro.
// Uso: MSKIA [CARPETA]  (la de DEJAVU.TTF, PNGTEST.PNG e icudt77l.dat; si
// no, la del programa). Al final, «MSKIA: todo bien (N pruebas)» y código 0.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <unistd.h>
#include <cpuid.h>
#include <zlib.h>
#include <unicode/putil.h>
#include "include/core/SkCanvas.h"
#include "include/core/SkSurface.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#include "include/core/SkPathBuilder.h"
#include "include/core/SkRRect.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkTypeface.h"
#include "include/core/SkTextBlob.h"
#include "include/core/SkImage.h"
#include "include/core/SkData.h"
#include "include/core/SkStream.h"
#include "include/core/SkMaskFilter.h"
#include "include/core/SkBlurTypes.h"
#include "include/core/SkPixmap.h"
#include "include/core/SkGraphics.h"
#include "include/core/SkMilestone.h"
#include "include/effects/SkGradient.h"
#include "include/effects/SkDashPathEffect.h"
#include "include/effects/SkRuntimeEffect.h"
#include "include/pathops/SkPathOps.h"
#include "include/codec/SkCodec.h"
#include "include/codec/SkPngDecoder.h"
#include "include/codec/SkJpegDecoder.h"
#include "include/codec/SkWebpDecoder.h"
#include "include/encode/SkPngEncoder.h"
#include "include/encode/SkJpegEncoder.h"
#include "include/encode/SkWebpEncoder.h"
#include "include/ports/SkFontMgr_empty.h"
#include "modules/skshaper/include/SkShaper.h"
#include "modules/skshaper/include/SkShaper_harfbuzz.h"
#include "modules/skunicode/include/SkUnicode_icu.h"

static int bien_, mal_;
#define MIRA(c, d) do { if (c) bien_++; else { mal_++; std::printf("  MAL: %s (línea %d)\n", (d), __LINE__); std::fflush(stdout); } } while (0)

static const int AN = 400, AL = 240;

// el camino que elige Skia (src/core/SkCpu.cpp y SkOpts.cpp): ML4 (AVX-512
// F, DQ, CD, BW y VL, con el sistema guardando los zmm), ML3 (AVX2, BMI1,
// BMI2, F16C y FMA, con el sistema guardando los ymm) o SSE2
static const char *camino()
{
	unsigned a, b, c, d;
	__cpuid(1, a, b, c, d);
	if (!(c & (1u << 27)) || !(c & (1u << 28))) return "SSE2";
	unsigned lo, hi;
	__asm__ __volatile__("xgetbv" : "=a"(lo), "=d"(hi) : "c"(0));
	if ((lo & 6) != 6) return "SSE2";
	bool fma = c & (1u << 12), f16c = c & (1u << 29);
	__cpuid_count(7, 0, a, b, c, d);
	bool avx2 = b & (1u << 5), bmi1 = b & (1u << 3), bmi2 = b & (1u << 8);
	bool ml4 = (b & (1u << 16)) && (b & (1u << 17)) && (b & (1u << 28)) && (b & (1u << 30)) && (b & (1u << 31)) && (lo & 0xE0) == 0xE0;
	if (avx2 && bmi1 && bmi2 && fma && f16c) return ml4 ? "ML4" : "ML3";
	return "SSE2";
}

static unsigned long crc(const void *p, size_t n) { return crc32(0, static_cast<const Bytef *>(p), static_cast<uInt>(n)); }

static sk_sp<SkData> lee(const std::string &ruta) { return SkData::MakeFromFileName(ruta.c_str()); }

int main(int argc, char **argv)
{
	std::setvbuf(stdout, nullptr, _IOLBF, 0);
	std::string carpeta;
	if (argc > 1) carpeta = argv[1];
	else {
		char e[512];
		ssize_t r = readlink("/proc/self/exe", e, sizeof e - 1);
		if (r > 0) { e[r] = 0; char *b = std::strrchr(e, '/'); if (b) *b = 0; carpeta = e; }
		else carpeta = ".";
	}
	u_setDataDirectory((carpeta.empty() ? std::string("/") : carpeta + "/").c_str());
	std::printf("MSKIA: Skia m%d (la de Ladybird) con el procesador, sin GPU; el camino: %s\n", SK_MILESTONE, camino());
	SkGraphics::Init();

	auto sup = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(AN, AL));
	MIRA(sup != nullptr, "SkSurfaces::Raster");
	if (!sup) { std::printf("MSKIA: %d mal\n", mal_); return 1; }
	SkCanvas *c = sup->getCanvas();
	c->clear(SK_ColorWHITE);

	// el degradado de fondo (arriba)
	{
		SkColor4f col[3] = { {0.95f, 0.85f, 0.2f, 1}, {0.85f, 0.2f, 0.25f, 1}, {0.2f, 0.3f, 0.8f, 1} };
		SkPoint pts[2] = { {0, 0}, {AN, 0} };
		SkPaint p;
		p.setShader(SkShaders::LinearGradient(pts, SkGradient(SkGradient::Colors(col, SkTileMode::kClamp), {})));
		c->drawRect(SkRect::MakeXYWH(0, 0, AN, 40), p);
	}
	// una sombra difuminada y un rectángulo redondeado encima
	{
		SkPaint s;
		s.setAntiAlias(true);
		s.setColor(SkColorSetARGB(160, 0, 0, 0));
		s.setMaskFilter(SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, 3.0f));
		c->drawRRect(SkRRect::MakeRectXY(SkRect::MakeXYWH(14, 54, 90, 50), 10, 10), s);
		SkPaint p;
		p.setAntiAlias(true);
		p.setColor(SkColorSetRGB(40, 140, 70));
		c->drawRRect(SkRRect::MakeRectXY(SkRect::MakeXYWH(10, 50, 90, 50), 10, 10), p);
	}
	// un círculo con borde
	{
		SkPaint p;
		p.setAntiAlias(true);
		p.setColor(SkColorSetRGB(250, 200, 0));
		c->drawCircle(150, 75, 24, p);
		p.setStyle(SkPaint::kStroke_Style);
		p.setStrokeWidth(3);
		p.setColor(SkColorSetRGB(120, 60, 0));
		c->drawCircle(150, 75, 24, p);
	}
	// una estrella con curvas y la unión de dos caminos (SkPathOps)
	{
		SkPathBuilder e;
		for (int i = 0; i < 10; i++) {
			float r = (i & 1) ? 11 : 26, a = (float)M_PI * i / 5 - (float)M_PI / 2;
			SkPoint q = { 215 + r * std::cos(a), 75 + r * std::sin(a) };
			if (i == 0) e.moveTo(q); else e.lineTo(q);
		}
		e.close();
		SkPathBuilder cur;
		cur.moveTo(250, 100).cubicTo(260, 40, 300, 110, 310, 52).lineTo(310, 100).close();
		SkPath u;
		bool ok = Op(e.detach(), cur.detach(), kUnion_SkPathOp, &u);
		MIRA(ok && !u.isEmpty(), "SkPathOps: la unión");
		SkPaint p;
		p.setAntiAlias(true);
		p.setColor(SkColorSetRGB(150, 40, 160));
		c->drawPath(u, p);
	}
	// una línea de rayas
	{
		SkPaint p;
		p.setAntiAlias(true);
		p.setStyle(SkPaint::kStroke_Style);
		p.setStrokeWidth(2);
		p.setColor(SkColorSetRGB(30, 30, 30));
		float iv[2] = { 8, 4 };
		p.setPathEffect(SkDashPathEffect::Make(iv, 0));
		c->drawLine(10, 116, 390, 116, p);
	}
	// un sombreador de SkSL (lo compila Skia ahora)
	{
		auto [ef, err] = SkRuntimeEffect::MakeForShader(SkString(
			"half4 main(float2 p) { float d = distance(p, float2(362, 80)); return half4(half(p.x / 400.0), half(0.6 - d / 80.0), 0.7, 1); }"));
		MIRA(ef != nullptr, "SkRuntimeEffect: el sombreador de SkSL");
		if (!ef) std::printf("  SkSL: %s\n", err.c_str());
		else {
			SkPaint p;
			p.setShader(ef->makeShader(nullptr, {}));
			c->drawRect(SkRect::MakeXYWH(330, 48, 64, 64), p);
		}
	}
	// PNGTEST.PNG, al doble
	int pan = 0, pal = 0;
	{
		auto d = lee(carpeta + "/PNGTEST.PNG");
		MIRA(d && SkPngDecoder::IsPng(d->data(), d->size()), "PNGTEST.PNG");
		if (d) {
			auto codec = SkPngDecoder::Decode(d, nullptr);
			MIRA(codec != nullptr, "SkPngDecoder");
			if (codec) {
				auto [img, res] = codec->getImage();
				MIRA(img && res == SkCodec::kSuccess, "SkCodec::getImage");
				if (img) {
					pan = img->width(); pal = img->height();
					c->drawImageRect(img, SkRect::MakeXYWH(230, 122, pan * 1.5f, pal * 1.5f), SkSamplingOptions(SkFilterMode::kLinear));
				}
			}
		}
	}
	// el texto
	const char *frase = "¡Señor Muñoz, qué pingüino tan añejo! Ñandú y árbol.";
	int glifos = 0;
	{
		auto mgr = SkFontMgr_New_Custom_Empty();
		auto tf = mgr->makeFromFile((carpeta + "/DEJAVU.TTF").c_str());
		MIRA(tf != nullptr, "DEJAVU.TTF");
		if (tf) {
			SkFont f(tf, 13);
			f.setEdging(SkFont::Edging::kAntiAlias);
			f.setSubpixel(true);
			glifos = f.countText(frase, std::strlen(frase), SkTextEncoding::kUTF8);
			SkPaint p;
			p.setAntiAlias(true);
			p.setColor(SK_ColorBLACK);
			// con HarfBuzz (SkShaper) y SkUnicode sobre ICU
			auto uni = SkUnicodes::ICU::Make();
			MIRA(uni != nullptr, "SkUnicodes::ICU");
			auto sh = SkShapers::HB::ShapeDontWrapOrReorder(uni, nullptr);
			MIRA(sh != nullptr, "SkShapers::HB");
			if (sh) {
				SkTextBlobBuilderRunHandler h(frase, {0, 0});
				sh->shape(frase, std::strlen(frase), f, true, 1e9f, &h);
				auto blob = h.makeBlob();
				MIRA(blob != nullptr, "SkShaper: el texto colocado");
				if (blob) c->drawTextBlob(blob, 6, 30, p);
				std::printf("  HarfBuzz (SkShaper) con DejaVu Sans: %d glifos, %d de ancho\n", glifos, blob ? (int)std::lround(blob->bounds().width()) : 0);
			}
			// con SkFont a secas
			SkFont g(tf, 18);
			g.setEdging(SkFont::Edging::kAntiAlias);
			p.setColor(SkColorSetRGB(20, 40, 120));
			const char *t2 = "Año 2026: Hispa-64";
			c->drawString(t2, 10, 140, g, p);
			c->drawString("ñ Ñ á é í ó ú ü ç ¿?", 10, 165, g, p);
		}
	}

	// lo dibujado
	SkPixmap pm;
	MIRA(sup->peekPixels(&pm), "peekPixels");
	const uint32_t *px = static_cast<const uint32_t *>(pm.addr());
	size_t n = (size_t)AN * AL * 4;
	unsigned long cp = crc(px, n);
	auto en = [&](int x, int y) { SkColor k = pm.getColor(x, y); return k; };
	MIRA(en(55, 75) == SkColorSetRGB(40, 140, 70), "el rectángulo redondeado, de su color");
	MIRA(en(150, 75) == SkColorSetRGB(250, 200, 0), "el círculo, de su color");
	MIRA(en(215, 75) == SkColorSetRGB(150, 40, 160), "la estrella, de su color");
	int oscuros = 0;
	for (int y = 18; y < 36; y++) for (int x = 0; x < AN; x++) { SkColor k = en(x, y); if (SkColorGetR(k) + SkColorGetG(k) + SkColorGetB(k) < 200) oscuros++; }
	MIRA(oscuros > 150, "el texto, dibujado");
	// la huella: la media de cada cuadro de 16 x 16, en RGB
	std::string hu;
	for (int by = 0; by < AL / 16; by++)
		for (int bx = 0; bx < AN / 16; bx++) {
			unsigned s[3] = {0, 0, 0};
			for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) {
				SkColor k = en(bx * 16 + x, by * 16 + y);
				s[0] += SkColorGetR(k); s[1] += SkColorGetG(k); s[2] += SkColorGetB(k);
			}
			char t[8];
			std::snprintf(t, sizeof t, "%02x%02x%02x", s[0] / 256, s[1] / 256, s[2] / 256);
			hu += t;
		}

	// a PNG, a /tmp/mskia.png, y otra vez
	SkPngEncoder::Options po;
	auto png = SkPngEncoder::Encode(pm, po);
	MIRA(png != nullptr, "SkPngEncoder");
	unsigned long cpng = png ? crc(png->data(), png->size()) : 0;
	{
		SkFILEWStream f("/tmp/mskia.png");
		MIRA(f.isValid() && png && f.write(png->data(), png->size()), "guardado en /tmp/mskia.png");
	}
	{
		auto d = lee("/tmp/mskia.png");
		auto codec = d ? SkPngDecoder::Decode(d, nullptr) : nullptr;
		bool igual = false;
		if (codec) {
			auto [img, res] = codec->getImage(pm.info());
			SkPixmap p2;
			if (img && img->peekPixels(&p2)) igual = std::memcmp(p2.addr(), px, n) == 0;
		}
		MIRA(igual, "/tmp/mskia.png, leído otra vez: los mismos píxeles");
	}
	// a JPEG y a WebP, y de vuelta
	{
		SkJpegEncoder::Options jo;
		jo.fQuality = 90;
		SkDynamicMemoryWStream w;
		bool ok = SkJpegEncoder::Encode(&w, pm, jo);
		auto d = w.detachAsData();
		auto codec = ok ? SkJpegDecoder::Decode(d, nullptr) : nullptr;
		auto [img, res] = codec ? codec->getImage() : std::make_tuple(sk_sp<SkImage>(), SkCodec::kInvalidInput);
		MIRA(img && img->width() == AN && img->height() == AL, "SkJpegEncoder y SkJpegDecoder");
		SkWebpEncoder::Options wo;
		wo.fCompression = SkWebpEncoder::Compression::kLossless;
		SkDynamicMemoryWStream w2;
		bool ok2 = SkWebpEncoder::Encode(&w2, pm, wo);
		auto d2 = w2.detachAsData();
		auto codec2 = ok2 ? SkWebpDecoder::Decode(d2, nullptr) : nullptr;
		bool igual = false;
		if (codec2) {
			auto [img2, res2] = codec2->getImage(pm.info());
			SkPixmap p2;
			if (img2 && img2->peekPixels(&p2)) igual = std::memcmp(p2.addr(), px, n) == 0;
		}
		MIRA(igual, "SkWebpEncoder sin pérdidas y SkWebpDecoder: los mismos píxeles");
		std::printf("  JPEG (calidad 90) y WebP sin pérdidas, de ida y vuelta\n");
	}
	std::printf("  PNGTEST.PNG de %d x %d (SkPngDecoder), a 1,5 veces con filtro lineal; un degradado, formas, una sombra, SkPathOps y SkSL\n", pan, pal);
	std::printf("  dibujado (%s): %d x %d, los píxeles con crc32 %08lx; a PNG en %zu octetos (crc32 %08lx), en /tmp/mskia.png\n",
	            camino(), AN, AL, cp, png ? png->size() : 0, cpng);
	std::printf("  huella: %s\n", hu.c_str());
	if (mal_) std::printf("MSKIA: %d mal de %d\n", mal_, bien_ + mal_);
	else std::printf("MSKIA: todo bien (%d pruebas)\n", bien_);
	return mal_ ? 1 : 0;
}
