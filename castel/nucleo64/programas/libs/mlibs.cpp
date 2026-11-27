// Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
// nucleo64/programas/libs/mlibs.cpp - Hispa-64, etapa 10 e (docs/64/HISPA-64.md):
// MLIBS, la prueba de las bibliotecas de terceros que pide Ladybird (hazlibs.sh):
// simdutf, fast_float, zmij, mimalloc, sqlite, woff2 (con brotli), wuffs (GIF),
// dav1d y libavif (AVIF), libpsl, simdjson, zstd y libtiff. Cada una, con su
// línea; los resultados, exactos y los mismos en Linux (el programa es
// estático: corre también en el anfitrión) y en Hispa-64. Lee sus datos
// (letra.woff2 y colores.avif) de su carpeta y escribe solo en /tmp.
// Acaba con «MLIBS: todo bien (N pruebas)» y el código 0, o «MLIBS: K mal» y 1.
#include <simdutf.h>
#include <fast_float/fast_float.h>
#include <zmij.h>
#include <mimalloc.h>
#include <sqlite3.h>
#include <woff2/decode.h>
#include <woff2/output.h>
#include <wuffs-v0.4.c>
#include <avif/avif.h>
#include <dav1d/dav1d.h>
#include <libpsl.h>
#include <simdjson.h>
#include <zstd.h>
#include <tiffio.h>

#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>
#include <unistd.h>

static int bien = 0, mal = 0;
static void mira(bool c, const char *qué)
{
	if (c) bien++;
	else { mal++; printf("  MAL: %s\n", qué); }
}

static uint64_t fnv(const void *p, size_t n, uint64_t h = 0xcbf29ce484222325ULL)
{
	const uint8_t *b = (const uint8_t *)p;
	for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 0x100000001b3ULL; }
	return h;
}

static std::string carpeta;   // la del programa (de argv[0]), con «/» al final
static std::vector<uint8_t> lee(const char *nombre)
{
	std::vector<uint8_t> v;
	std::string r = carpeta + nombre;
	FILE *f = fopen(r.c_str(), "rb");
	if (!f) f = fopen(nombre, "rb");
	if (!f) return v;
	uint8_t b[65536];
	size_t n;
	while ((n = fread(b, 1, sizeof b, f)) > 0) v.insert(v.end(), b, b + n);
	fclose(f);
	return v;
}

static const char TEXTO[] = "¡Hola, España! El pingüino Ñandú comió ñoquis a las 9:30; «año» ≠ «ano»; €, ç, ß, 漢字 y 🐧.";

// ---- simdutf ----
static void prueba_simdutf()
{
	size_t n = strlen(TEXTO);
	bool ok = simdutf::validate_utf8(TEXTO, n);
	size_t n16 = simdutf::utf16_length_from_utf8(TEXTO, n);
	size_t n32 = simdutf::utf32_length_from_utf8(TEXTO, n);
	std::vector<char16_t> u16(n16);
	size_t e16 = simdutf::convert_utf8_to_utf16le(TEXTO, n, u16.data());
	std::vector<char> vuelta(simdutf::utf8_length_from_utf16le(u16.data(), e16));
	size_t e8 = simdutf::convert_utf16le_to_utf8(u16.data(), e16, vuelta.data());
	bool igual = e8 == n && !memcmp(vuelta.data(), TEXTO, n);
	// uno malo: «ñ» cortada a la mitad
	std::string malo = std::string("añ").substr(0, 2) + "o";
	simdutf::result r = simdutf::validate_utf8_with_errors(malo.data(), malo.size());
	// base64
	char b64[256];
	size_t nb = simdutf::binary_to_base64("Ñandú", strlen("Ñandú"), b64);
	b64[nb] = 0;
	char bin[64];
	simdutf::result rb = simdutf::base64_to_binary(b64, nb, bin);
	bool b64bien = rb.error == simdutf::error_code::SUCCESS && rb.count == strlen("Ñandú") && !memcmp(bin, "Ñandú", rb.count);
	// lo mismo con cada implementación que tiene este procesador
	int impl = 0, impl_bien = 0;
	for (auto i : simdutf::get_available_implementations()) {
		if (!i->supported_by_runtime_system()) continue;
		impl++;
		std::vector<char16_t> w(n16);
		size_t k = i->convert_utf8_to_utf16le(TEXTO, n, w.data());
		if (i->validate_utf8(TEXTO, n) && k == e16 && !memcmp(w.data(), u16.data(), k * 2) && !i->validate_utf8(malo.data(), malo.size())) impl_bien++;
	}
	mira(ok && igual && n16 == e16, "simdutf: UTF-8 a UTF-16 y vuelta");
	mira(r.error == simdutf::error_code::TOO_SHORT && r.count == 1, "simdutf: el UTF-8 malo");
	mira(b64bien && !strcmp(b64, "w5FhbmTDug=="), "simdutf: base64");
	mira(impl > 0 && impl == impl_bien, "simdutf: cada implementación da lo mismo");
	printf("  simdutf %s: %zu octetos de UTF-8 válidos, %zu unidades de UTF-16 y %zu caracteres, ida y vuelta: %s; «añ» cortada: %s en el octeto %zu; «Ñandú» en base64: %s; cada implementación del procesador, lo mismo\n",
	       SIMDUTF_VERSION, n, n16, n32, ok && igual ? "igual" : "MAL", r.error == simdutf::error_code::TOO_SHORT ? "TOO_SHORT" : "otro error", r.count, b64);
}

// ---- fast_float ----
static void prueba_fast_float()
{
	struct { const char *t; uint64_t bits; } c[] = {
		{"3.141592653589793", 0x400921fb54442d18ULL},
		{"6.02214076e23", 0x44dfe185ca57c517ULL},
		{"0.1", 0x3fb999999999999aULL},
		{"4.9406564584124654e-324", 0x1ULL},
		{"1.7976931348623157e308", 0x7fefffffffffffffULL},
		{"-0", 0x8000000000000000ULL},
	};
	int b = 0;
	for (auto &x : c) {
		double d = 0;
		auto r = fast_float::from_chars(x.t, x.t + strlen(x.t), d);
		uint64_t u;
		memcpy(&u, &d, 8);
		if (r.ec == std::errc() && u == x.bits && r.ptr == x.t + strlen(x.t)) b++;
	}
	// con la coma decimal (como en español)
	const char *es = "1234,5678";
	double d = 0;
	fast_float::parse_options op{fast_float::chars_format::general, ','};
	auto r = fast_float::from_chars_advanced(es, es + strlen(es), d, op);
	float f = 0;
	const char *tf = "16777217";
	fast_float::from_chars(tf, tf + strlen(tf), f);
	mira(b == 6, "fast_float: los dobles");
	mira(r.ec == std::errc() && d == 1234.5678, "fast_float: la coma decimal");
	mira(f == 16777216.0f, "fast_float: el redondeo a float");
	printf("  fast_float 8.3.3: %d de 6 dobles con sus bits exactos (pi, Avogadro, 0,1, el subnormal más pequeño, el mayor, -0); «1234,5678» con la coma: %.4f; «16777217» en float: %.0f\n",
	       b, d, (double)f);
}

// ---- zmij ----
static void prueba_zmij()
{
	struct { double v; const char *t; } c[] = {
		{0.1, "0.1"}, {1e23, "1e+23"}, {5e-324, "5e-324"}, {1.7976931348623157e308, "1.7976931348623157e+308"},
		{123.456, "123.456"}, {-2.5, "-2.5"}, {0.3, "0.3"}, {100, "100"},
	};
	int b = 0;
	std::string todo;
	for (auto &x : c) {
		char buf[zmij::buffer_sizes<double>::shortest + 1];
		char *e = zmij::write(buf, sizeof buf - 1, x.v);
		*e = 0;
		if (!strcmp(buf, x.t)) b++;
		else printf("  zmij: %s en vez de %s\n", buf, x.t);
		if (!todo.empty()) todo += " ";
		todo += buf;
	}
	mira(b == 8, "zmij: la forma más corta");
	printf("  zmij 1.2: la forma más corta de 8 números: %s (%d de 8 bien)\n", todo.c_str(), b);
}

// ---- mimalloc ----
static void prueba_mimalloc()
{
	std::atomic<int> fallos{0};
	std::atomic<long> octetos{0};
	auto trabajo = [&](int semilla) {
		uint32_t x = 2463534242u + semilla;
		std::vector<std::pair<uint8_t *, size_t>> v;
		for (int i = 0; i < 20000; i++) {
			x ^= x << 13; x ^= x >> 17; x ^= x << 5;
			size_t n = 1 + x % ((x & 0x100) ? 65536 : 256);
			uint8_t *p = (uint8_t *)mi_malloc(n);
			if (!p || mi_usable_size(p) < n) { fallos++; continue; }
			memset(p, (uint8_t)(i + semilla), n);
			v.push_back({p, n});
			octetos += n;
			if (v.size() > 500) {
				auto q = v[x % v.size()];
				if (q.first[q.second - 1] != q.first[0]) fallos++;
				mi_free(q.first);
				v[x % v.size()] = v.back();
				v.pop_back();
			}
		}
		for (auto &q : v) mi_free(q.first);
	};
	std::vector<std::thread> h;
	for (int i = 0; i < 4; i++) h.emplace_back(trabajo, i);
	for (auto &t : h) t.join();
	void *a = mi_malloc_aligned(1000, 4096);
	bool alineado = a && ((uintptr_t)a & 4095) == 0;
	mi_free(a);
	char *r = (char *)mi_malloc(10);
	strcpy(r, "ñandú");
	r = (char *)mi_realloc(r, 1 << 20);
	bool realloc_bien = r && !strcmp(r, "ñandú");
	mi_free(r);
	mi_heap_t *heap = mi_heap_new();
	int en_heap = 0;
	for (int i = 0; i < 1000; i++) if (mi_heap_malloc(heap, 100 + i)) en_heap++;
	mi_heap_destroy(heap);
	mira(fallos == 0 && octetos > 0, "mimalloc: 4 hilos");
	mira(alineado, "mimalloc: alineado a 4096");
	mira(realloc_bien, "mimalloc: realloc");
	mira(en_heap == 1000, "mimalloc: el montón propio");
	printf("  mimalloc %d.%d.%d (MI_SECURE=4): 4 hilos con 20.000 bloques cada uno, de 1 octeto a 64 KB, %s; alineado a 4096: %s; realloc a 1 MB con su contenido: %s; un montón propio con 1.000 bloques, destruido\n",
	       mi_version() / 100, mi_version() / 10 % 10, mi_version() % 10, fallos == 0 ? "intactos" : "MAL", alineado ? "sí" : "no", realloc_bien ? "bien" : "MAL");
}

// ---- sqlite ----
static int sql_cuenta(sqlite3 *db, const char *q, std::string *texto = nullptr)
{
	sqlite3_stmt *st;
	int n = -1;
	if (sqlite3_prepare_v2(db, q, -1, &st, nullptr) != SQLITE_OK) return -2;
	if (sqlite3_step(st) == SQLITE_ROW) {
		n = sqlite3_column_int(st, 0);
		if (texto) {
			const unsigned char *t = sqlite3_column_text(st, 0);
			*texto = t ? (const char *)t : "";
		}
	}
	sqlite3_finalize(st);
	return n;
}

static void prueba_sqlite()
{
	sqlite3 *db;
	bool ok = sqlite3_open(":memory:", &db) == SQLITE_OK;
	ok = ok && sqlite3_exec(db,
		"CREATE TABLE socios(id INTEGER PRIMARY KEY, nombre TEXT, ciudad TEXT, cuota REAL);"
		"INSERT INTO socios(nombre, ciudad, cuota) VALUES ('Muñoz', 'Logroño', 12.5), ('Peña', 'A Coruña', 20), ('Ibáñez', 'Cádiz', 7.25), ('García', 'Logroño', 30);"
		"CREATE VIRTUAL TABLE notas USING fts5(texto);"
		"INSERT INTO notas VALUES ('el pingüino come ñoquis'), ('la cigüeña vuela'), ('ñoquis con tomate');",
		nullptr, nullptr, nullptr) == SQLITE_OK;
	std::string nombres, js, suma;
	int n = sql_cuenta(db, "SELECT count(*) FROM socios WHERE ciudad = 'Logroño'");
	sql_cuenta(db, "SELECT group_concat(nombre, ',') FROM (SELECT nombre FROM socios ORDER BY nombre)", &nombres);
	sql_cuenta(db, "SELECT json_object('ciudad', ciudad, 'n', count(*)) FROM socios GROUP BY ciudad ORDER BY ciudad LIMIT 1", &js);
	sql_cuenta(db, "SELECT printf('%.2f', sum(cuota)) FROM socios", &suma);
	int fts = sql_cuenta(db, "SELECT count(*) FROM notas WHERE notas MATCH 'ñoquis'");
	sqlite3_close(db);
	mira(ok && n == 2 && nombres == "García,Ibáñez,Muñoz,Peña" && suma == "69.75", "sqlite: en memoria");
	mira(js == "{\"ciudad\":\"A Coruña\",\"n\":1}", "sqlite: JSON");
	mira(fts == 2, "sqlite: FTS5");
	// un archivo en /tmp (con «unix-none»: sin los cerrojos de fcntl, que Hispa-64 aún no tiene)
	const char *ruta = "/tmp/mlibs.db";
	unlink(ruta);
	int filas = -1;
	std::string integ;
	bool okf = sqlite3_open_v2(ruta, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, "unix-none") == SQLITE_OK;
	if (okf) {
		okf = sqlite3_exec(db, "CREATE TABLE t(a INTEGER, b TEXT); BEGIN;", nullptr, nullptr, nullptr) == SQLITE_OK;
		for (int i = 0; okf && i < 2000; i++) {
			char q[128];
			snprintf(q, sizeof q, "INSERT INTO t VALUES(%d, 'fila ñ número %d');", i, i);
			okf = sqlite3_exec(db, q, nullptr, nullptr, nullptr) == SQLITE_OK;
		}
		okf = okf && sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr) == SQLITE_OK;
		sqlite3_close(db);
	}
	if (okf && sqlite3_open_v2(ruta, &db, SQLITE_OPEN_READONLY, "unix-none") == SQLITE_OK) {
		filas = sql_cuenta(db, "SELECT count(*) FROM t WHERE b LIKE '%ñ%'");
		sql_cuenta(db, "PRAGMA integrity_check", &integ);
		sqlite3_close(db);
	}
	unlink(ruta);
	mira(okf && filas == 2000 && integ == "ok", "sqlite: un archivo en /tmp");
	printf("  sqlite %s: en memoria, 4 socios (ordenados: %s; de Logroño, %d; la suma de las cuotas, %s), JSON %s, FTS5 «ñoquis»: %d; en /tmp, 2.000 filas en una transacción, cerrado y abierto otra vez: %d, integrity_check: %s\n",
	       sqlite3_libversion(), nombres.c_str(), n, suma.c_str(), js.c_str(), fts, filas, integ.c_str());
}

// ---- woff2 (con brotli) ----
static void prueba_woff2()
{
	std::vector<uint8_t> w = lee("letra.woff2");
	size_t fin = w.empty() ? 0 : woff2::ComputeWOFF2FinalSize(w.data(), w.size());
	std::string ttf(std::min(fin, (size_t)(30 << 20)), '\0');
	woff2::WOFF2StringOut out(&ttf);
	bool ok = !w.empty() && woff2::ConvertWOFF2ToTTF(w.data(), w.size(), &out);
	ttf.resize(out.Size());
	int tablas = ttf.size() > 12 ? ((uint8_t)ttf[4] << 8 | (uint8_t)ttf[5]) : 0;
	bool sfnt = ttf.size() > 4 && !memcmp(ttf.data(), "\0\1\0\0", 4);
	bool tiene_glyf = ttf.find("glyf") != std::string::npos && ttf.find("cmap") != std::string::npos;
	// uno roto: no debe pasar
	std::vector<uint8_t> roto(w.begin(), w.begin() + w.size() / 2);   // cortado a la mitad
	std::string r2(1 << 20, '\0');
	woff2::WOFF2StringOut o2(&r2);
	bool rechaza = !roto.empty() && !woff2::ConvertWOFF2ToTTF(roto.data(), roto.size(), &o2);
	mira(ok && sfnt && tiene_glyf && tablas > 5, "woff2: letra.woff2 a TTF");
	mira(rechaza, "woff2: el roto, rechazado");
	printf("  woff2 1.0.2 (con brotli %s): letra.woff2, %zu octetos, a TrueType: %zu octetos, %d tablas (glyf y cmap rehechas), FNV %016llx; cortado a la mitad, rechazado: %s\n",
	       "1.1.0", w.size(), ttf.size(), tablas, (unsigned long long)fnv(ttf.data(), ttf.size()), rechaza ? "sí" : "no");
}

// ---- wuffs (GIF) ----
// Un GIF de 8 x 4 con 4 colores y dos cuadros, hecho aquí: el LZW «sin
// comprimir» (un CLEAR cada dos píxeles: los códigos, siempre de 3 bits).
struct Bits {
	std::vector<uint8_t> o;
	uint32_t acc = 0;
	int n = 0;
	void pon(uint32_t c, int w) { acc |= c << n; n += w; while (n >= 8) { o.push_back(acc & 255); acc >>= 8; n -= 8; } }
	void fin() { if (n) o.push_back(acc & 255); acc = 0; n = 0; }
};
static void gif_cuadro(std::vector<uint8_t> &g, const uint8_t *px, int w, int h)
{
	uint8_t gce[] = {0x21, 0xF9, 4, 0, 10, 0, 0, 0};   // 0,1 s
	g.insert(g.end(), gce, gce + 8);
	uint8_t id[] = {0x2C, 0, 0, 0, 0, (uint8_t)w, 0, (uint8_t)h, 0, 0};
	g.insert(g.end(), id, id + 10);
	g.push_back(2);   // el tamaño mínimo de código: 2 (4 colores; CLEAR = 4, EOI = 5)
	Bits b;
	for (int i = 0; i < w * h; i++) {
		if (i % 2 == 0) b.pon(4, 3);
		b.pon(px[i], 3);
	}
	b.pon(5, 3);
	b.fin();
	for (size_t i = 0; i < b.o.size(); i += 255) {
		size_t k = std::min((size_t)255, b.o.size() - i);
		g.push_back((uint8_t)k);
		g.insert(g.end(), b.o.begin() + i, b.o.begin() + i + k);
	}
	g.push_back(0);
}

static void prueba_wuffs()
{
	const int W = 8, H = 4;
	uint8_t pal[12] = {0, 0, 0, 255, 0, 0, 0, 160, 0, 30, 60, 255};
	uint8_t c1[W * H], c2[W * H];
	for (int i = 0; i < W * H; i++) { c1[i] = (i + i / W) % 4; c2[i] = 3 - c1[i]; }
	std::vector<uint8_t> g = {'G', 'I', 'F', '8', '9', 'a', W, 0, H, 0, 0x81, 0, 0};
	g.insert(g.end(), pal, pal + 12);
	uint8_t ns[] = {0x21, 0xFF, 11, 'N', 'E', 'T', 'S', 'C', 'A', 'P', 'E', '2', '.', '0', 3, 1, 0, 0, 0};
	g.insert(g.end(), ns, ns + sizeof ns);
	gif_cuadro(g, c1, W, H);
	gif_cuadro(g, c2, W, H);
	g.push_back(0x3B);

	wuffs_gif__decoder *d = wuffs_gif__decoder__alloc();
	wuffs_base__io_buffer src = wuffs_base__ptr_u8__reader(g.data(), g.size(), true);
	wuffs_base__image_config ic;
	wuffs_base__status st = wuffs_gif__decoder__decode_image_config(d, &ic, &src);
	bool ok = !st.repr && wuffs_base__pixel_config__width(&ic.pixcfg) == W && wuffs_base__pixel_config__height(&ic.pixcfg) == H;
	std::vector<uint8_t> px(W * H * 4);
	wuffs_base__pixel_config__set(&ic.pixcfg, WUFFS_BASE__PIXEL_FORMAT__RGBA_NONPREMUL, WUFFS_BASE__PIXEL_SUBSAMPLING__NONE, W, H);
	wuffs_base__pixel_buffer pb;
	ok = ok && !wuffs_base__pixel_buffer__set_from_slice(&pb, &ic.pixcfg, wuffs_base__make_slice_u8(px.data(), px.size())).repr;
	std::vector<uint8_t> wb(wuffs_gif__decoder__workbuf_len(d).max_incl + 1);
	int cuadros = 0, iguales = 0;
	for (; ok;) {
		wuffs_base__frame_config fc;
		st = wuffs_gif__decoder__decode_frame_config(d, &fc, &src);
		if (st.repr == wuffs_base__note__end_of_data) break;
		if (st.repr) { ok = false; break; }
		st = wuffs_gif__decoder__decode_frame(d, &pb, &src, WUFFS_BASE__PIXEL_BLEND__SRC, wuffs_base__make_slice_u8(wb.data(), wb.size()), nullptr);
		if (st.repr) { ok = false; break; }
		const uint8_t *c = cuadros == 0 ? c1 : c2;
		bool igual = true;
		for (int i = 0; i < W * H; i++)
			if (px[i * 4] != pal[c[i] * 3] || px[i * 4 + 1] != pal[c[i] * 3 + 1] || px[i * 4 + 2] != pal[c[i] * 3 + 2] || px[i * 4 + 3] != 255) igual = false;
		if (igual) iguales++;
		cuadros++;
	}
	uint32_t bucles = wuffs_gif__decoder__num_animation_loops(d);
	free(d);
	mira(ok && cuadros == 2 && iguales == 2, "wuffs: el GIF de dos cuadros");
	printf("  wuffs %s: un GIF de 8 x 4 con dos cuadros (%zu octetos): %d cuadros, %d con sus 32 píxeles exactos, %u vueltas (para siempre: 0)\n",
	       WUFFS_VERSION_STRING, g.size(), cuadros, iguales, bucles);
}

// ---- dav1d y libavif ----
static void prueba_avif()
{
	std::vector<uint8_t> a = lee("colores.avif");
	avifDecoder *d = avifDecoderCreate();
	d->codecChoice = AVIF_CODEC_CHOICE_DAV1D;
	d->maxThreads = 2;
	bool ok = !a.empty() && avifDecoderSetIOMemory(d, a.data(), a.size()) == AVIF_RESULT_OK && avifDecoderParse(d) == AVIF_RESULT_OK;
	int cuadros = 0;
	uint64_t h = 0xcbf29ce484222325ULL;
	uint32_t w = 0, al = 0, prof = 0;
	while (ok && avifDecoderNextImage(d) == AVIF_RESULT_OK) {
		avifRGBImage rgb;
		avifRGBImageSetDefaults(&rgb, d->image);
		rgb.format = AVIF_RGB_FORMAT_RGBA;
		rgb.depth = 8;
		if (avifRGBImageAllocatePixels(&rgb) != AVIF_RESULT_OK || avifImageYUVToRGB(d->image, &rgb) != AVIF_RESULT_OK) { ok = false; break; }
		for (uint32_t y = 0; y < rgb.height; y++) h = fnv(rgb.pixels + y * rgb.rowBytes, rgb.width * 4, h);
		w = rgb.width; al = rgb.height; prof = d->image->depth;
		avifRGBImageFreePixels(&rgb);
		cuadros++;
	}
	int esperados = d->imageCount;
	avifDecoderDestroy(d);
	mira(ok && cuadros > 1 && cuadros == esperados && w > 0, "libavif: el AVIF animado");
	printf("  libavif %s con dav1d %s (en C, sin ensamblador): colores.avif, %zu octetos: %d cuadros de %u x %u, %u bits, a RGBA: FNV %016llx\n",
	       avifVersion(), dav1d_version(), a.size(), cuadros, w, al, prof, (unsigned long long)h);
}

// ---- libpsl ----
static void prueba_psl()
{
	const psl_ctx_t *p = psl_builtin();
	struct { const char *n; const char *reg; } c[] = {
		{"www.jubilats.org.es", "jubilats.org.es"},
		{"www.example.co.uk", "example.co.uk"},
		{"carlos.github.io", "carlos.github.io"},
		{"a.b.ayuntamiento.es", "ayuntamiento.es"},
		{"www.xn--and-6ma2c.es", "xn--and-6ma2c.es"},
	};
	int b = 0;
	for (auto &x : c) {
		const char *r = psl_registrable_domain(p, x.n);
		if (r && !strcmp(r, x.reg)) b++;
		else printf("  libpsl: %s -> %s\n", x.n, r ? r : "(nada)");
	}
	bool sufijos = psl_is_public_suffix(p, "co.uk") && psl_is_public_suffix(p, "github.io") && !psl_is_public_suffix(p, "example.es")
		&& psl_is_public_suffix(p, "es") && psl_is_public_suffix2(p, "foo.ck", PSL_TYPE_ANY) && !psl_is_public_suffix(p, "www.ck");
	mira(p && b == 5, "libpsl: los dominios registrables");
	mira(sufijos, "libpsl: los sufijos públicos (con «*.ck» y «!www.ck»)");
	printf("  libpsl %s: la lista de dentro, %d sufijos (%d con «*»); 5 de 5 dominios registrables bien (www.jubilats.org.es: %s); co.uk, github.io, es y foo.ck son sufijos, www.ck no: %s\n",
	       psl_get_version(), psl_suffix_count(p), psl_suffix_wildcard_count(p), psl_registrable_domain(p, "www.jubilats.org.es"), sufijos ? "bien" : "MAL");
}

// ---- simdjson ----
static void prueba_simdjson()
{
	const char *js = R"({"nombre": "Muñoz Ibáñez", "edad": 77, "socio": true, "cuotas": [12.5, 20, 3e2], "ciudad": "Logroño", "nota": "ñ y 🐧"})";
	simdjson::padded_string ps(js, strlen(js));
	simdjson::dom::parser p;
	simdjson::dom::element doc;
	bool ok = !p.parse(ps).get(doc);
	std::string_view vn, vt;
	int64_t edad = 0;
	double suma = 0;
	ok = ok && !doc["nombre"].get(vn) && !doc["edad"].get(edad) && !doc["nota"].get(vt);
	std::string nombre(vn), nota(vt);   // (lo de «parser» vale hasta el siguiente parse)
	simdjson::dom::array arr;
	if (ok && !doc["cuotas"].get(arr)) for (auto v : arr) { double x; if (!v.get(x)) suma += x; }
	// On-Demand
	simdjson::ondemand::parser op;
	simdjson::padded_string ps2(js, strlen(js));
	std::string_view vc;
	auto od = op.iterate(ps2);
	bool ok2 = !od["ciudad"].get_string().get(vc);
	std::string ciudad(vc);
	// minify y uno malo
	std::vector<char> min(strlen(js));
	size_t nmin = 0;
	bool okm = simdjson::minify(js, strlen(js), min.data(), nmin) == simdjson::SUCCESS;
	simdjson::padded_string malo(std::string("{\"a\": [1, 2,, 3]}"));
	simdjson::dom::element dm;
	bool rechaza = p.parse(malo).get(dm) != simdjson::SUCCESS;
	int impl = 0, impl_bien = 0;
	for (auto i : simdjson::get_available_implementations()) {
		if (!i->supported_by_runtime_system()) continue;
		impl++;
		if (i->validate_utf8(js, strlen(js))) impl_bien++;
	}
	mira(ok && nombre == "Muñoz Ibáñez" && edad == 77 && suma == 332.5 && nota == "ñ y 🐧", "simdjson: DOM");
	mira(ok2 && ciudad == "Logroño", "simdjson: On-Demand");
	mira(okm && rechaza && impl > 0 && impl == impl_bien, "simdjson: minify, el malo y cada implementación");
	printf("  simdjson %s: «%.*s», %lld años, cuotas que suman %.1f, la nota con \\u: «%.*s»; On-Demand: «%.*s»; minify: %zu de %zu octetos; «[1, 2,, 3]», rechazado: %s\n",
	       SIMDJSON_VERSION, (int)nombre.size(), nombre.data(), (long long)edad, suma, (int)nota.size(), nota.data(), (int)ciudad.size(), ciudad.data(), nmin, strlen(js), rechaza ? "sí" : "no");
}

// ---- zstd ----
static void prueba_zstd()
{
	std::string t;
	for (int i = 0; t.size() < 200000; i++) { t += TEXTO; t += " "; t += std::to_string(i * 7919 % 1000); t += "\n"; }
	std::vector<char> c(ZSTD_compressBound(t.size()));
	size_t nc = ZSTD_compress(c.data(), c.size(), t.data(), t.size(), 3);
	std::vector<char> v(t.size());
	size_t nd = ZSTD_isError(nc) ? 0 : ZSTD_decompress(v.data(), v.size(), c.data(), nc);
	bool ok = !ZSTD_isError(nc) && nd == t.size() && !memcmp(v.data(), t.data(), nd);
	// de 1 KB en 1 KB, como llegaría de la red
	ZSTD_DStream *ds = ZSTD_createDStream();
	ZSTD_initDStream(ds);
	std::string s;
	char ob[4096];
	for (size_t i = 0; ok && i < nc; i += 1024) {
		ZSTD_inBuffer in = {c.data() + i, std::min((size_t)1024, nc - i), 0};
		while (in.pos < in.size) {
			ZSTD_outBuffer out = {ob, sizeof ob, 0};
			if (ZSTD_isError(ZSTD_decompressStream(ds, &out, &in))) { ok = false; break; }
			s.append(ob, out.pos);
		}
	}
	ZSTD_freeDStream(ds);
	bool trozos = s == t;
	mira(ok, "zstd: comprimir y descomprimir");
	mira(trozos, "zstd: a trozos");
	printf("  zstd %s: %zu octetos de texto a %zu (nivel 3), FNV %016llx, y vuelta igual; a trozos de 1 KB: %s\n",
	       ZSTD_versionString(), t.size(), nc, (unsigned long long)fnv(c.data(), ZSTD_isError(nc) ? 0 : nc), trozos ? "igual" : "MAL");
}

// ---- libtiff ----
static void calla(const char *, const char *, va_list) {}
static void prueba_tiff()
{
	TIFFSetWarningHandler(calla);
	const int W = 64, H = 48;
	std::vector<uint8_t> img(W * H * 3);
	for (int y = 0; y < H; y++)
		for (int x = 0; x < W; x++) {
			uint8_t *p = &img[(y * W + x) * 3];
			p[0] = x * 4; p[1] = y * 5; p[2] = (x ^ y) * 3;
		}
	const char *ruta = "/tmp/mlibs.tif";
	bool ok = false;
	long tam = 0;
	uint64_t h = 0;
	std::string desc;
	TIFF *t = TIFFOpen(ruta, "w");
	if (t) {
		TIFFSetField(t, TIFFTAG_IMAGEWIDTH, W);
		TIFFSetField(t, TIFFTAG_IMAGELENGTH, H);
		TIFFSetField(t, TIFFTAG_BITSPERSAMPLE, 8);
		TIFFSetField(t, TIFFTAG_SAMPLESPERPIXEL, 3);
		TIFFSetField(t, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
		TIFFSetField(t, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
		TIFFSetField(t, TIFFTAG_COMPRESSION, COMPRESSION_LZW);
		TIFFSetField(t, TIFFTAG_PREDICTOR, PREDICTOR_HORIZONTAL);
		TIFFSetField(t, TIFFTAG_ROWSPERSTRIP, 16);
		TIFFSetField(t, TIFFTAG_IMAGEDESCRIPTION, "Peña de Logroño");
		ok = true;
		for (int y = 0; y < H && ok; y++) ok = TIFFWriteScanline(t, &img[y * W * 3], y, 0) == 1;
		TIFFClose(t);
	}
	FILE *f = fopen(ruta, "rb");
	if (f) {
		std::vector<uint8_t> b(1 << 16);
		tam = fread(b.data(), 1, b.size(), f);
		h = fnv(b.data(), tam);
		fclose(f);
	}
	int iguales = 0;
	t = ok ? TIFFOpen(ruta, "r") : nullptr;
	if (t) {
		std::vector<uint32_t> r(W * H);
		char *d = nullptr;
		if (TIFFGetField(t, TIFFTAG_IMAGEDESCRIPTION, &d) && d) desc = d;
		if (TIFFReadRGBAImageOriented(t, W, H, r.data(), ORIENTATION_TOPLEFT, 0))
			for (int i = 0; i < W * H; i++)
				if (TIFFGetR(r[i]) == img[i * 3] && TIFFGetG(r[i]) == img[i * 3 + 1] && TIFFGetB(r[i]) == img[i * 3 + 2] && TIFFGetA(r[i]) == 255) iguales++;
		TIFFClose(t);
	}
	unlink(ruta);
	mira(ok && iguales == W * H && desc == "Peña de Logroño", "libtiff: escribir y leer");
	printf("  libtiff %s: 64 x 48 RGB con LZW y predictor a /tmp: %ld octetos, FNV %016llx; leído otra vez: %d de %d píxeles iguales, la descripción «%s»\n",
	       "4.7.2", tam, (unsigned long long)h, iguales, W * H, desc.c_str());
}

int main(int argc, char **argv)
{
	setvbuf(stdout, nullptr, _IOLBF, 0);
	const char *b = argc > 0 ? strrchr(argv[0], '/') : nullptr;
	carpeta = b ? std::string(argv[0], b - argv[0] + 1) : std::string("/");
	printf("MLIBS: las bibliotecas de terceros que pide Ladybird (C y C++ con musl, %s)\n", __VERSION__);
	prueba_simdutf();
	prueba_fast_float();
	prueba_zmij();
	prueba_mimalloc();
	prueba_sqlite();
	prueba_woff2();
	prueba_wuffs();
	prueba_avif();
	prueba_psl();
	prueba_simdjson();
	prueba_zstd();
	prueba_tiff();
	if (mal) { printf("MLIBS: %d mal (y %d bien)\n", mal, bien); return 1; }
	printf("MLIBS: todo bien (%d pruebas)\n", bien);
	return 0;
}
