// Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
// nucleo64/programas/cxx/micu.cpp - Hispa-64, etapa 10 b (docs/64/HISPA-64.md,
// sección 19): la prueba de ICU 77.1 (terceros/icu), compilada con cxx.sh (la
// libstdc++ con musl), lo que pedirá el navegador para el texto: normalizar,
// mayúsculas y minúsculas, ordenar a la española (y a la tradicional), cortar
// líneas, palabras y letras (grafemas), los números, las fechas y las zonas,
// los plurales, IDNA (los nombres con eñe), BiDi, windows-1252 y las
// expresiones regulares. Los datos, en icudt77l.dat (los de terceros/icu/data:
// español, catalán, gallego, euskera e inglés; ICU_DATA, o la carpeta del
// programa). Dice «MICU: todo bien (N pruebas)» y unas líneas con lo que da,
// que son las mismas en Linux (pruebas.sh lo corre también allí).
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstdarg>
#include <unistd.h>
#include <string>
#include <vector>
#include <algorithm>
#include <memory>
#include <unicode/uversion.h>
#include <unicode/uchar.h>
#include <unicode/uscript.h>
#include <unicode/putil.h>
#include <unicode/udata.h>
#include <unicode/unistr.h>
#include <unicode/normalizer2.h>
#include <unicode/coll.h>
#include <unicode/brkiter.h>
#include <unicode/numfmt.h>
#include <unicode/numberformatter.h>
#include <unicode/datefmt.h>
#include <unicode/timezone.h>
#include <unicode/calendar.h>
#include <unicode/plurrule.h>
#include <unicode/uidna.h>
#include <unicode/ubidi.h>
#include <unicode/ucnv.h>
#include <unicode/regex.h>
#include <unicode/locid.h>
#include <unicode/ucasemap.h>
#include "../musl/comun.h"

using icu::UnicodeString;

static std::string u8(const UnicodeString &s)
{
	std::string r;
	s.toUTF8String(r);
	return r;
}

static UnicodeString U(const char *s) { return UnicodeString::fromUTF8(s); }

// una línea de resultados: lo que no sean ASCII y las letras de aquí que
// la consola de Hispa-64 pasa tal cual a la página 437 (á é í ó ú ñ ü ç « »
// ¿ ¡ ª º), como \u{XXXX}: así sale lo mismo por el puerto serie que en
// Linux (pruebas.sh compara las dos)
static void linea(const char *f, ...)
{
	static const char32_t bien[] = U"áéíóúñÑüÜçÇ«»¿¡ªº";
	char b[2048];
	va_list a;
	va_start(a, f);
	vsnprintf(b, sizeof b, f, a);
	va_end(a);
	UnicodeString t = UnicodeString::fromUTF8(b);
	std::string r;
	for (int32_t i = 0; i < t.length(); i = t.moveIndex32(i, 1)) {
		UChar32 c = t.char32At(i);
		bool vale = c < 0x80;
		for (int k = 0; !vale && bien[k]; k++) vale = (UChar32)bien[k] == c;
		if (vale) { UnicodeString x(c); x.toUTF8String(r); }
		else { char e[16]; snprintf(e, sizeof e, "\\u{%04X}", (unsigned)c); r += e; }
	}
	fputs(r.c_str(), stdout);
}

int main(int argc, char **argv)
{
	UErrorCode e = U_ZERO_ERROR;
	setvbuf(stdout, nullptr, _IOLBF, 0);
	// los datos: ICU_DATA, o la carpeta del programa
	if (!getenv("ICU_DATA")) {
		static char d[512];
		const char *b = strrchr(argv[0], '/');
		if (b) snprintf(d, sizeof d, "%.*s", (int)(b - argv[0] + 1), argv[0]);
		else snprintf(d, sizeof d, "/");
		u_setDataDirectory(d);
	}
	(void)argc;
	UVersionInfo v, uv;
	char vt[U_MAX_VERSION_STRING_LENGTH], ut[U_MAX_VERSION_STRING_LENGTH];
	u_getVersion(v);
	u_versionToString(v, vt);
	u_getUnicodeVersion(uv);
	u_versionToString(uv, ut);
	printf("MICU: ICU %s (Unicode %s), con musl y la libstdc++ (cxx.sh)\n", vt, ut);
	MIRA(!strcmp(vt, "77.1") && !strncmp(ut, "16.0", 4), "la versión de ICU y de Unicode");
	// los datos, abiertos
	{
		UErrorCode e2 = U_ZERO_ERROR;
		UDataMemory *m = udata_open(nullptr, "res", "root", &e2);
		MIRA(U_SUCCESS(e2), "los datos (icudt77l.dat)");
		if (m) udata_close(m);
		if (U_FAILURE(e2)) { printf("  sin los datos en «%s»: %s\n", u_getDataDirectory(), u_errorName(e2)); return acaba("MICU"); }
	}

	// ---- normalizar ----
	const icu::Normalizer2 *nfc = icu::Normalizer2::getNFCInstance(e);
	const icu::Normalizer2 *nfd = icu::Normalizer2::getNFDInstance(e);
	const icu::Normalizer2 *nfkc = icu::Normalizer2::getNFKCInstance(e);
	MIRA(U_SUCCESS(e) && nfc->normalize(U("e\xCC\x81"), e) == U("é") && nfd->normalize(U("ñ"), e) == U("n\xCC\x83")
	     && nfkc->normalize(U("ﬁ①"), e) == U("fi1") && nfc->isNormalized(U("año"), e), "normalizar: NFC, NFD y NFKC");

	// ---- mayúsculas y minúsculas ----
	MIRA(UnicodeString(U("straße")).toUpper(icu::Locale("es")) == U("STRASSE"), "mayúsculas: ß -> SS");
	MIRA(UnicodeString(U("istanbul")).toUpper(icu::Locale("tr")) == U("İSTANBUL"), "mayúsculas en turco: i -> İ");
	MIRA(UnicodeString(U("ΟΔΟΣ")).toLower(icu::Locale("el")) == U("\xCE\xBF\xCE\xB4\xCE\xBF\xCF\x82"), "minúsculas: la sigma final (ς)");
	MIRA(UnicodeString(U("ÑANDÚ")).foldCase() == U("ñandú"), "foldCase");
	{
		UErrorCode e3 = U_ZERO_ERROR;
		UCaseMap *cm = ucasemap_open("es", 0, &e3);
		char t[64];
		int32_t n = ucasemap_utf8ToTitle(cm, t, sizeof t, "el niño de la ciudad", -1, &e3);
		MIRA(U_SUCCESS(e3) && n > 0 && !strcmp(t, "El Niño De La Ciudad"), "ucasemap_utf8ToTitle");
		ucasemap_close(cm);
	}

	// ---- ordenar ----
	{
		std::unique_ptr<icu::Collator> es(icu::Collator::createInstance(icu::Locale("es"), e));
		std::unique_ptr<icu::Collator> tr(icu::Collator::createInstance(icu::Locale("es", "", "TRADITIONAL"), e));
		std::unique_ptr<icu::Collator> tr2(icu::Collator::createInstance(icu::Locale("es@collation=traditional"), e));
		std::vector<std::string> p = { "zapato", "ñu", "nube", "Ávila", "avión", "chico", "cosa", "Nuño", "nuez", "llama", "luz" };
		auto ordena = [&](icu::Collator *c) {
			std::vector<std::string> q = p;
			std::sort(q.begin(), q.end(), [&](const std::string &a, const std::string &b) {
				UErrorCode x = U_ZERO_ERROR;
				return c->compareUTF8(a, b, x) == UCOL_LESS;
			});
			std::string r;
			for (auto &s : q) r += (r.empty() ? "" : " ") + s;
			return r;
		};
		std::string a = U_SUCCESS(e) ? ordena(es.get()) : "", b = U_SUCCESS(e) ? ordena(tr2.get()) : "";
		linea("  a la española: %s\n  a la tradicional: %s\n", a.c_str(), b.c_str());
		MIRA(a == "Ávila avión chico cosa llama luz nube nuez Nuño ñu zapato", "ordenar a la española (la ñ después de la n)");
		MIRA(b == "Ávila avión cosa chico luz llama nube nuez Nuño ñu zapato", "ordenar a la tradicional (ch y ll aparte)");
		UErrorCode x = U_ZERO_ERROR;
		es->setStrength(icu::Collator::PRIMARY);
		MIRA(es->compare(U("canción"), U("CANCION"), x) == UCOL_EQUAL && es->compare(U("cana"), U("caña"), x) == UCOL_LESS, "ordenar: sin tildes ni mayúsculas (la ñ sigue siendo otra letra)");
		(void)tr;
	}

	// ---- cortar: letras (grafemas), palabras y líneas ----
	{
		UErrorCode x = U_ZERO_ERROR;
		std::unique_ptr<icu::BreakIterator> ch(icu::BreakIterator::createCharacterInstance(icu::Locale("es"), x));
		UnicodeString t = U("🇪🇸👩‍👩‍👧e\xCC\x81ñ");
		ch->setText(t);
		int n = 0;
		for (int32_t p = ch->next(); p != icu::BreakIterator::DONE; p = ch->next()) n++;
		linea("  letras: %d grafemas en %d unidades de UTF-16 (una bandera, una familia con ZWJ, e + tilde y ñ)\n", n, t.length());
		MIRA(U_SUCCESS(x) && n == 4 && t.length() == 15, "letras: una bandera, una familia (ZWJ), e + tilde y ñ: 4 grafemas");
		std::unique_ptr<icu::BreakIterator> pa(icu::BreakIterator::createWordInstance(icu::Locale("es"), x));
		UnicodeString f = U("¿Qué tal? El navegador de Hispa-64 corta 3,5 palabras.");
		pa->setText(f);
		std::string pal;
		int32_t i0 = pa->first();
		for (int32_t p = pa->next(); p != icu::BreakIterator::DONE; i0 = p, p = pa->next())
			if (pa->getRuleStatus() != UBRK_WORD_NONE) pal += (pal.empty() ? "" : "|") + u8(UnicodeString(f, i0, p - i0));
		linea("  palabras: %s\n", pal.c_str());
		MIRA(pal == "Qué|tal|El|navegador|de|Hispa|64|corta|3,5|palabras", "palabras");
		std::unique_ptr<icu::BreakIterator> li(icu::BreakIterator::createLineInstance(icu::Locale("es"), x));
		UnicodeString l = U("Un texto largo, con «comillas» y guiones-de-unión, que se parte (bien).");
		li->setText(l);
		std::string tr;
		i0 = li->first();
		for (int32_t p = li->next(); p != icu::BreakIterator::DONE; i0 = p, p = li->next())
			tr += (tr.empty() ? "" : "/") + u8(UnicodeString(l, i0, p - i0));
		linea("  líneas: %s\n", tr.c_str());
		MIRA(tr == "Un /texto /largo, /con /«comillas» /y /guiones-/de-/unión, /que /se /parte /(bien).", "dónde se puede cortar la línea (UAX 14)");
	}

	// ---- números ----
	{
		UErrorCode x = U_ZERO_ERROR;
		UnicodeString s;
		std::unique_ptr<icu::NumberFormat> n(icu::NumberFormat::createInstance(icu::Locale("es_ES"), x));
		n->format(1234567.891, s);
		std::unique_ptr<icu::NumberFormat> c(icu::NumberFormat::createCurrencyInstance(icu::Locale("es_ES"), x));
		UnicodeString m, m2, s4;
		n->format(1234.0, s4);
		UnicodeString s5 = icu::number::NumberFormatter::withLocale(icu::Locale("es_ES")).formatDouble(1234.0, x).toString(x);
		UnicodeString s6 = icu::number::NumberFormatter::withLocale(icu::Locale("es_ES")).formatDouble(12345.0, x).toString(x);
		c->format(98765.5, m);
		c->format(1234.5, m2);
		std::unique_ptr<icu::NumberFormat> en(icu::NumberFormat::createInstance(icu::Locale("en_US"), x));
		UnicodeString se;
		en->format(1234567.891, se);
		linea("  números: %s; %s (NumberFormat), %s y %s (NumberFormatter); %s; %s; en inglés, %s\n", u8(s).c_str(), u8(s4).c_str(), u8(s5).c_str(), u8(s6).c_str(), u8(m).c_str(), u8(m2).c_str(), u8(se).c_str());
		MIRA(U_SUCCESS(x) && u8(s) == "1.234.567,891" && u8(s4) == "1.234" && u8(s5) == "1234" && u8(s6) == "12.345" && u8(m) == "98.765,50\xC2\xA0€" && u8(m2) == "1.234,50\xC2\xA0€" && u8(se) == "1,234,567.891",
		     "números a la española (con NumberFormatter, 1234 sin el punto de los miles: dos cifras como mínimo, lo de CLDR), euros y en inglés");
		icu::Formattable fo;
		n->parse(U("3,25"), fo, x);
		MIRA(U_SUCCESS(x) && fo.getDouble() == 3.25, "leer «3,25»");
	}

	// ---- fechas y zonas ----
	{
		UErrorCode x = U_ZERO_ERROR;
		std::unique_ptr<icu::TimeZone> z(icu::TimeZone::createTimeZone("Europe/Madrid"));
		UDate inv = 1795635000000.0, ver = 1784143800000.0;     // 25/11/2026 19:30 y 15/07/2026 19:30 (UTC)
		int32_t o1, d1, o2, d2;
		z->getOffset(inv, false, o1, d1, x);
		z->getOffset(ver, false, o2, d2, x);
		MIRA(U_SUCCESS(x) && o1 == 3600000 && d1 == 0 && o2 == 3600000 && d2 == 3600000, "Europa/Madrid: UTC+1 en noviembre, UTC+2 en julio");
		std::unique_ptr<icu::DateFormat> f(icu::DateFormat::createDateTimeInstance(icu::DateFormat::kFull, icu::DateFormat::kShort, icu::Locale("es_ES")));
		f->setTimeZone(*z);
		UnicodeString s, s2, s3;
		f->format(inv, s);
		std::unique_ptr<icu::DateFormat> fc(icu::DateFormat::createDateInstance(icu::DateFormat::kLong, icu::Locale("ca_ES")));
		fc->setTimeZone(*z);
		fc->format(ver, s2);
		std::unique_ptr<icu::DateFormat> fg(icu::DateFormat::createDateInstance(icu::DateFormat::kLong, icu::Locale("gl_ES")));
		fg->setTimeZone(*z);
		fg->format(ver, s3);
		linea("  la fecha: %s; en catalán, %s; en gallego, %s\n", u8(s).c_str(), u8(s2).c_str(), u8(s3).c_str());
		MIRA(u8(s).find("miércoles, 25 de noviembre de 2026") == 0 && u8(s).find("20:30") != std::string::npos, "la fecha en español (la hora de Madrid)");
		MIRA(u8(s2) == "15 de juliol del 2026" && u8(s3) == "15 de xullo de 2026", "la fecha en catalán («del 2026») y en gallego");
		std::unique_ptr<icu::Calendar> ca(icu::Calendar::createInstance(*z, icu::Locale("es_ES"), x));
		MIRA(U_SUCCESS(x) && ca->getFirstDayOfWeek(x) == UCAL_MONDAY, "la semana empieza el lunes");
	}

	// ---- la zona del sistema: ICU la saca de readlink de /etc/localtime (con
	// TZ en la forma de POSIX); en Linux es la del anfitrión: no se compara ----
	{
		char l[160];
		ssize_t k = readlink("/etc/localtime", l, sizeof l - 1);
		l[k > 0 ? k : 0] = 0;
		std::unique_ptr<icu::TimeZone> z(icu::TimeZone::createDefault());
		UnicodeString id;
		z->getID(id);
		printf("MICU: la zona del sistema: %s (readlink de /etc/localtime: %s)\n", u8(id).c_str(), k > 0 ? l : "no es un enlace");
		MIRA(id.length() > 0 && id != U("Etc/Unknown"), "la zona del sistema");
	}

	// ---- plurales ----
	{
		UErrorCode x = U_ZERO_ERROR;
		std::unique_ptr<icu::PluralRules> p(icu::PluralRules::forLocale(icu::Locale("es"), x));
		MIRA(U_SUCCESS(x) && p->select(1.0) == U("one") && p->select(2.0) == U("other") && p->select(1000000.0) == U("many"), "los plurales del español (uno, otros, «many» del millón)");
	}

	// ---- IDNA (UTS 46): los nombres con eñe ----
	{
		UErrorCode x = U_ZERO_ERROR;
		UIDNA *id = uidna_openUTS46(UIDNA_NONTRANSITIONAL_TO_ASCII | UIDNA_NONTRANSITIONAL_TO_UNICODE | UIDNA_CHECK_BIDI, &x);
		char a[256], u[256];
		UIDNAInfo i1 = UIDNA_INFO_INITIALIZER, i2 = UIDNA_INFO_INITIALIZER;
		uidna_nameToASCII_UTF8(id, "ñandú.Español.es", -1, a, sizeof a, &i1, &x);
		uidna_nameToUnicodeUTF8(id, a, -1, u, sizeof u, &i2, &x);
		linea("  IDNA: ñandú.Español.es -> %s -> %s\n", a, u);
		MIRA(U_SUCCESS(x) && i1.errors == 0 && !strcmp(a, "xn--and-6ma2c.xn--espaol-zwa.es") && !strcmp(u, "ñandú.español.es"), "IDNA: a ASCII (punycode) y de vuelta");
		uidna_close(id);
	}

	// ---- BiDi, escrituras, windows-1252, expresiones regulares, nombres de idiomas ----
	{
		UErrorCode x = U_ZERO_ERROR;
		UnicodeString t = U("abc שלום def 123");
		UBiDi *b = ubidi_openSized(t.length(), 0, &x);
		ubidi_setPara(b, t.getBuffer(), t.length(), UBIDI_DEFAULT_LTR, nullptr, &x);
		int32_t runs = ubidi_countRuns(b, &x);
		MIRA(U_SUCCESS(x) && runs == 3 && ubidi_getDirection(b) == UBIDI_MIXED, "BiDi: hebreo dentro de un texto de izquierda a derecha (3 tramos)");
		ubidi_close(b);
		MIRA(uscript_getScript(0x00F1, &x) == USCRIPT_LATIN && uscript_getScript(0x05E9, &x) == USCRIPT_HEBREW && u_isalpha(0x00F1) && u_charType(0x20AC) == U_CURRENCY_SYMBOL, "las escrituras y las clases de las letras");
		UConverter *cv = ucnv_open("windows-1252", &x);
		UChar w[8];
		int32_t n = ucnv_toUChars(cv, w, 8, "\x80\xF1", 2, &x);
		MIRA(U_SUCCESS(x) && n == 2 && w[0] == 0x20AC && w[1] == 0x00F1, "windows-1252: 0x80 es el euro");
		ucnv_close(cv);
		icu::RegexMatcher r(U("(\\p{L}+)@(\\p{L}+)\\.es"), 0, x);
		UnicodeString co = U("escribe a doña.ñandú@españa.es ya");      // (el RegexMatcher no copia el texto)
		r.reset(co);
		MIRA(U_SUCCESS(x) && r.find() && u8(r.group(1, x)) == "ñandú" && u8(r.group(2, x)) == "españa", "expresiones regulares de ICU (\\p{L})");
		UnicodeString nom, nom2;
		icu::Locale("ca").getDisplayName(icu::Locale("es"), nom);
		icu::Locale("eu").getDisplayName(icu::Locale("es"), nom2);
		linea("  los idiomas: %s, %s\n", u8(nom).c_str(), u8(nom2).c_str());
		MIRA(u8(nom) == "catalán" && u8(nom2) == "euskera", "el nombre de los idiomas en español");
	}
	return acaba("MICU");
}
