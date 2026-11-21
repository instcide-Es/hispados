/* fuente.c - Hispa-PDF: las letras de los PDF. Qué letra (Unicode) es cada
 * código (ToUnicode, /Encoding con sus /Differences, WinAnsi, MacRoman y
 * la estándar), lo que mide (/Widths, /W) y su dibujo: la letra que trae el
 * PDF si es TrueType u OpenType (con stb_truetype, terceros/stb) y, si no,
 * la DejaVu que más se le parezca (con el ancho de la suya).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include <stdlib.h>
#include "fuente.h"

#define STBTT_ifloor(x) ((int)so_suelo(x))
#define STBTT_iceil(x) ((int)so_techo(x))
#define STBTT_sqrt(x) so_raiz(x)
#define STBTT_pow(x, y) so_pot(x, y)
#define STBTT_fmod(x, y) so_resto(x, y)
#define STBTT_cos(x) so_cos(x)
#define STBTT_acos(x) so_acos(x)
#define STBTT_fabs(x) so_abs(x)
#define STBTT_malloc(x, u) ((void)(u), so_pide(x))
#define STBTT_free(x, u) ((void)(u), so_suelta(x))
#define STBTT_assert(x) ((void)0)
#define STBTT_strlen(x) strlen(x)
#define STBTT_memcpy memcpy
#define STBTT_memset memset
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

static const u16 win_ansi[256] = {
	0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007, 0x0008, 0x0009, 0x000A, 0x000B,
	0x000C, 0x000D, 0x000E, 0x000F, 0x0010, 0x0011, 0x0012, 0x0013, 0x0014, 0x0015, 0x0016, 0x0017,
	0x0018, 0x0019, 0x001A, 0x001B, 0x001C, 0x001D, 0x001E, 0x001F, 0x0020, 0x0021, 0x0022, 0x0023,
	0x0024, 0x0025, 0x0026, 0x0027, 0x0028, 0x0029, 0x002A, 0x002B, 0x002C, 0x002D, 0x002E, 0x002F,
	0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037, 0x0038, 0x0039, 0x003A, 0x003B,
	0x003C, 0x003D, 0x003E, 0x003F, 0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047,
	0x0048, 0x0049, 0x004A, 0x004B, 0x004C, 0x004D, 0x004E, 0x004F, 0x0050, 0x0051, 0x0052, 0x0053,
	0x0054, 0x0055, 0x0056, 0x0057, 0x0058, 0x0059, 0x005A, 0x005B, 0x005C, 0x005D, 0x005E, 0x005F,
	0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067, 0x0068, 0x0069, 0x006A, 0x006B,
	0x006C, 0x006D, 0x006E, 0x006F, 0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077,
	0x0078, 0x0079, 0x007A, 0x007B, 0x007C, 0x007D, 0x007E, 0x007F, 0x20AC, 0x0000, 0x201A, 0x0192,
	0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x0000, 0x017D, 0x0000,
	0x0000, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A,
	0x0153, 0x0000, 0x017E, 0x0178, 0x00A0, 0x00A1, 0x00A2, 0x00A3, 0x00A4, 0x00A5, 0x00A6, 0x00A7,
	0x00A8, 0x00A9, 0x00AA, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x00AF, 0x00B0, 0x00B1, 0x00B2, 0x00B3,
	0x00B4, 0x00B5, 0x00B6, 0x00B7, 0x00B8, 0x00B9, 0x00BA, 0x00BB, 0x00BC, 0x00BD, 0x00BE, 0x00BF,
	0x00C0, 0x00C1, 0x00C2, 0x00C3, 0x00C4, 0x00C5, 0x00C6, 0x00C7, 0x00C8, 0x00C9, 0x00CA, 0x00CB,
	0x00CC, 0x00CD, 0x00CE, 0x00CF, 0x00D0, 0x00D1, 0x00D2, 0x00D3, 0x00D4, 0x00D5, 0x00D6, 0x00D7,
	0x00D8, 0x00D9, 0x00DA, 0x00DB, 0x00DC, 0x00DD, 0x00DE, 0x00DF, 0x00E0, 0x00E1, 0x00E2, 0x00E3,
	0x00E4, 0x00E5, 0x00E6, 0x00E7, 0x00E8, 0x00E9, 0x00EA, 0x00EB, 0x00EC, 0x00ED, 0x00EE, 0x00EF,
	0x00F0, 0x00F1, 0x00F2, 0x00F3, 0x00F4, 0x00F5, 0x00F6, 0x00F7, 0x00F8, 0x00F9, 0x00FA, 0x00FB,
	0x00FC, 0x00FD, 0x00FE, 0x00FF};
static const u16 mac_roman[256] = {
	0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007, 0x0008, 0x0009, 0x000A, 0x000B,
	0x000C, 0x000D, 0x000E, 0x000F, 0x0010, 0x0011, 0x0012, 0x0013, 0x0014, 0x0015, 0x0016, 0x0017,
	0x0018, 0x0019, 0x001A, 0x001B, 0x001C, 0x001D, 0x001E, 0x001F, 0x0020, 0x0021, 0x0022, 0x0023,
	0x0024, 0x0025, 0x0026, 0x0027, 0x0028, 0x0029, 0x002A, 0x002B, 0x002C, 0x002D, 0x002E, 0x002F,
	0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037, 0x0038, 0x0039, 0x003A, 0x003B,
	0x003C, 0x003D, 0x003E, 0x003F, 0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047,
	0x0048, 0x0049, 0x004A, 0x004B, 0x004C, 0x004D, 0x004E, 0x004F, 0x0050, 0x0051, 0x0052, 0x0053,
	0x0054, 0x0055, 0x0056, 0x0057, 0x0058, 0x0059, 0x005A, 0x005B, 0x005C, 0x005D, 0x005E, 0x005F,
	0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067, 0x0068, 0x0069, 0x006A, 0x006B,
	0x006C, 0x006D, 0x006E, 0x006F, 0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077,
	0x0078, 0x0079, 0x007A, 0x007B, 0x007C, 0x007D, 0x007E, 0x007F, 0x00C4, 0x00C5, 0x00C7, 0x00C9,
	0x00D1, 0x00D6, 0x00DC, 0x00E1, 0x00E0, 0x00E2, 0x00E4, 0x00E3, 0x00E5, 0x00E7, 0x00E9, 0x00E8,
	0x00EA, 0x00EB, 0x00ED, 0x00EC, 0x00EE, 0x00EF, 0x00F1, 0x00F3, 0x00F2, 0x00F4, 0x00F6, 0x00F5,
	0x00FA, 0x00F9, 0x00FB, 0x00FC, 0x2020, 0x00B0, 0x00A2, 0x00A3, 0x00A7, 0x2022, 0x00B6, 0x00DF,
	0x00AE, 0x00A9, 0x2122, 0x00B4, 0x00A8, 0x2260, 0x00C6, 0x00D8, 0x221E, 0x00B1, 0x2264, 0x2265,
	0x00A5, 0x00B5, 0x2202, 0x2211, 0x220F, 0x03C0, 0x222B, 0x00AA, 0x00BA, 0x03A9, 0x00E6, 0x00F8,
	0x00BF, 0x00A1, 0x00AC, 0x221A, 0x0192, 0x2248, 0x2206, 0x00AB, 0x00BB, 0x2026, 0x00A0, 0x00C0,
	0x00C3, 0x00D5, 0x0152, 0x0153, 0x2013, 0x2014, 0x201C, 0x201D, 0x2018, 0x2019, 0x00F7, 0x25CA,
	0x00FF, 0x0178, 0x2044, 0x20AC, 0x2039, 0x203A, 0xFB01, 0xFB02, 0x2021, 0x00B7, 0x201A, 0x201E,
	0x2030, 0x00C2, 0x00CA, 0x00C1, 0x00CB, 0x00C8, 0x00CD, 0x00CE, 0x00CF, 0x00CC, 0x00D3, 0x00D4,
	0xF8FF, 0x00D2, 0x00DA, 0x00DB, 0x00D9, 0x0131, 0x02C6, 0x02DC, 0x00AF, 0x02D8, 0x02D9, 0x02DA,
	0x00B8, 0x02DD, 0x02DB, 0x02C7};
static const u16 estandar[256] = {
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0020, 0x0021, 0x0022, 0x0023,
	0x0024, 0x0025, 0x0026, 0x2019, 0x0028, 0x0029, 0x002A, 0x002B, 0x002C, 0x002D, 0x002E, 0x002F,
	0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037, 0x0038, 0x0039, 0x003A, 0x003B,
	0x003C, 0x003D, 0x003E, 0x003F, 0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047,
	0x0048, 0x0049, 0x004A, 0x004B, 0x004C, 0x004D, 0x004E, 0x004F, 0x0050, 0x0051, 0x0052, 0x0053,
	0x0054, 0x0055, 0x0056, 0x0057, 0x0058, 0x0059, 0x005A, 0x005B, 0x005C, 0x005D, 0x005E, 0x005F,
	0x2018, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067, 0x0068, 0x0069, 0x006A, 0x006B,
	0x006C, 0x006D, 0x006E, 0x006F, 0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077,
	0x0078, 0x0079, 0x007A, 0x007B, 0x007C, 0x007D, 0x007E, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x00A1, 0x00A2, 0x00A3, 0x2044, 0x00A5, 0x0192, 0x00A7,
	0x00A4, 0x0027, 0x201C, 0x00AB, 0x2039, 0x203A, 0xFB01, 0xFB02, 0x0000, 0x2013, 0x2020, 0x2021,
	0x00B7, 0x0000, 0x00B6, 0x2022, 0x201A, 0x201E, 0x201D, 0x00BB, 0x2026, 0x2030, 0x0000, 0x00BF,
	0x0000, 0x0060, 0x00B4, 0x02C6, 0x02DC, 0x00AF, 0x02D8, 0x02D9, 0x00A8, 0x0000, 0x02DA, 0x00B8,
	0x0000, 0x02DD, 0x02DB, 0x02C7, 0x2014, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x00C6, 0x0000, 0x00AA,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0141, 0x00D8, 0x0152, 0x00BA, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x00E6, 0x0000, 0x0000, 0x0000, 0x0131, 0x0000, 0x0000, 0x0142, 0x00F8, 0x0153, 0x00DF,
	0x0000, 0x0000, 0x0000, 0x0000};
/* los nombres de las letras (de la lista de Adobe, lo de las lenguas de Europa), por orden */
static const struct { const char *n; u16 u; } nombres_letra[] = {
	{"A", 0x0041},
	{"AE", 0x00C6},
	{"Aacute", 0x00C1},
	{"Abreve", 0x0102},
	{"Acircumflex", 0x00C2},
	{"Adieresis", 0x00C4},
	{"Agrave", 0x00C0},
	{"Amacron", 0x0100},
	{"Aogonek", 0x0104},
	{"Aring", 0x00C5},
	{"Atilde", 0x00C3},
	{"B", 0x0042},
	{"C", 0x0043},
	{"Cacute", 0x0106},
	{"Ccaron", 0x010C},
	{"Ccedilla", 0x00C7},
	{"D", 0x0044},
	{"Dcaron", 0x010E},
	{"Dcroat", 0x0110},
	{"Delta", 0x0394},
	{"E", 0x0045},
	{"Eacute", 0x00C9},
	{"Ecaron", 0x011A},
	{"Ecircumflex", 0x00CA},
	{"Edieresis", 0x00CB},
	{"Egrave", 0x00C8},
	{"Emacron", 0x0112},
	{"Eogonek", 0x0118},
	{"Eth", 0x00D0},
	{"Euro", 0x20AC},
	{"F", 0x0046},
	{"G", 0x0047},
	{"Gbreve", 0x011E},
	{"H", 0x0048},
	{"I", 0x0049},
	{"Iacute", 0x00CD},
	{"Icircumflex", 0x00CE},
	{"Idieresis", 0x00CF},
	{"Idotaccent", 0x0130},
	{"Igrave", 0x00CC},
	{"Imacron", 0x012A},
	{"J", 0x004A},
	{"K", 0x004B},
	{"L", 0x004C},
	{"Lcaron", 0x013D},
	{"Lslash", 0x0141},
	{"M", 0x004D},
	{"N", 0x004E},
	{"Nacute", 0x0143},
	{"Ncaron", 0x0147},
	{"Ntilde", 0x00D1},
	{"O", 0x004F},
	{"OE", 0x0152},
	{"Oacute", 0x00D3},
	{"Ocircumflex", 0x00D4},
	{"Odieresis", 0x00D6},
	{"Ograve", 0x00D2},
	{"Ohungarumlaut", 0x0150},
	{"Omacron", 0x014C},
	{"Omega", 0x03A9},
	{"Oslash", 0x00D8},
	{"Otilde", 0x00D5},
	{"P", 0x0050},
	{"Q", 0x0051},
	{"R", 0x0052},
	{"Rcaron", 0x0158},
	{"S", 0x0053},
	{"Sacute", 0x015A},
	{"Scaron", 0x0160},
	{"Scedilla", 0x015E},
	{"T", 0x0054},
	{"Tcaron", 0x0164},
	{"Thorn", 0x00DE},
	{"U", 0x0055},
	{"Uacute", 0x00DA},
	{"Ucircumflex", 0x00DB},
	{"Udieresis", 0x00DC},
	{"Ugrave", 0x00D9},
	{"Uhungarumlaut", 0x0170},
	{"Umacron", 0x016A},
	{"Uring", 0x016E},
	{"V", 0x0056},
	{"W", 0x0057},
	{"X", 0x0058},
	{"Y", 0x0059},
	{"Yacute", 0x00DD},
	{"Ydieresis", 0x0178},
	{"Z", 0x005A},
	{"Zacute", 0x0179},
	{"Zcaron", 0x017D},
	{"Zdotaccent", 0x017B},
	{"a", 0x0061},
	{"aacute", 0x00E1},
	{"abreve", 0x0103},
	{"acircumflex", 0x00E2},
	{"acute", 0x00B4},
	{"adieresis", 0x00E4},
	{"ae", 0x00E6},
	{"agrave", 0x00E0},
	{"alpha", 0x03B1},
	{"amacron", 0x0101},
	{"ampersand", 0x0026},
	{"aogonek", 0x0105},
	{"approxequal", 0x2248},
	{"aring", 0x00E5},
	{"arrowdown", 0x2193},
	{"arrowleft", 0x2190},
	{"arrowright", 0x2192},
	{"arrowup", 0x2191},
	{"asciicircum", 0x005E},
	{"asciitilde", 0x007E},
	{"asterisk", 0x002A},
	{"at", 0x0040},
	{"atilde", 0x00E3},
	{"b", 0x0062},
	{"backslash", 0x005C},
	{"bar", 0x007C},
	{"beta", 0x03B2},
	{"braceleft", 0x007B},
	{"braceright", 0x007D},
	{"bracketleft", 0x005B},
	{"bracketright", 0x005D},
	{"breve", 0x02D8},
	{"brokenbar", 0x00A6},
	{"bullet", 0x2022},
	{"c", 0x0063},
	{"cacute", 0x0107},
	{"caron", 0x02C7},
	{"ccaron", 0x010D},
	{"ccedilla", 0x00E7},
	{"cedilla", 0x00B8},
	{"cent", 0x00A2},
	{"checkmark", 0x2713},
	{"circumflex", 0x02C6},
	{"colon", 0x003A},
	{"comma", 0x002C},
	{"copyright", 0x00A9},
	{"currency", 0x00A4},
	{"d", 0x0064},
	{"dagger", 0x2020},
	{"daggerdbl", 0x2021},
	{"dcaron", 0x010F},
	{"dcroat", 0x0111},
	{"degree", 0x00B0},
	{"delta", 0x03B4},
	{"dieresis", 0x00A8},
	{"divide", 0x00F7},
	{"dollar", 0x0024},
	{"dotaccent", 0x02D9},
	{"dotlessi", 0x0131},
	{"dotlessj", 0x0237},
	{"e", 0x0065},
	{"eacute", 0x00E9},
	{"ecaron", 0x011B},
	{"ecircumflex", 0x00EA},
	{"edieresis", 0x00EB},
	{"egrave", 0x00E8},
	{"eight", 0x0038},
	{"ellipsis", 0x2026},
	{"emacron", 0x0113},
	{"emdash", 0x2014},
	{"endash", 0x2013},
	{"eogonek", 0x0119},
	{"epsilon", 0x03B5},
	{"equal", 0x003D},
	{"eth", 0x00F0},
	{"exclam", 0x0021},
	{"exclamdown", 0x00A1},
	{"f", 0x0066},
	{"ff", 0xFB00},
	{"ffi", 0xFB03},
	{"ffl", 0xFB04},
	{"fi", 0xFB01},
	{"five", 0x0035},
	{"fl", 0xFB02},
	{"florin", 0x0192},
	{"four", 0x0034},
	{"fraction", 0x2044},
	{"g", 0x0067},
	{"gamma", 0x03B3},
	{"gbreve", 0x011F},
	{"germandbls", 0x00DF},
	{"grave", 0x0060},
	{"greater", 0x003E},
	{"greaterequal", 0x2265},
	{"guillemotleft", 0x00AB},
	{"guillemotright", 0x00BB},
	{"guilsinglleft", 0x2039},
	{"guilsinglright", 0x203A},
	{"h", 0x0068},
	{"hungarumlaut", 0x02DD},
	{"hyphen", 0x002D},
	{"i", 0x0069},
	{"iacute", 0x00ED},
	{"icircumflex", 0x00EE},
	{"idieresis", 0x00EF},
	{"igrave", 0x00EC},
	{"imacron", 0x012B},
	{"infinity", 0x221E},
	{"integral", 0x222B},
	{"j", 0x006A},
	{"k", 0x006B},
	{"l", 0x006C},
	{"lambda", 0x03BB},
	{"lcaron", 0x013E},
	{"less", 0x003C},
	{"lessequal", 0x2264},
	{"logicalnot", 0x00AC},
	{"lozenge", 0x25CA},
	{"lslash", 0x0142},
	{"m", 0x006D},
	{"macron", 0x00AF},
	{"middot", 0x00B7},
	{"minus", 0x2212},
	{"mu", 0x00B5},
	{"multiply", 0x00D7},
	{"n", 0x006E},
	{"nacute", 0x0144},
	{"nbspace", 0x00A0},
	{"ncaron", 0x0148},
	{"nine", 0x0039},
	{"notequal", 0x2260},
	{"ntilde", 0x00F1},
	{"numbersign", 0x0023},
	{"o", 0x006F},
	{"oacute", 0x00F3},
	{"ocircumflex", 0x00F4},
	{"odieresis", 0x00F6},
	{"oe", 0x0153},
	{"ogonek", 0x02DB},
	{"ograve", 0x00F2},
	{"ohungarumlaut", 0x0151},
	{"omacron", 0x014D},
	{"one", 0x0031},
	{"onehalf", 0x00BD},
	{"onequarter", 0x00BC},
	{"onesuperior", 0x00B9},
	{"ordfeminine", 0x00AA},
	{"ordmasculine", 0x00BA},
	{"oslash", 0x00F8},
	{"otilde", 0x00F5},
	{"p", 0x0070},
	{"paragraph", 0x00B6},
	{"parenleft", 0x0028},
	{"parenright", 0x0029},
	{"partialdiff", 0x2202},
	{"percent", 0x0025},
	{"period", 0x002E},
	{"periodcentered", 0x00B7},
	{"perthousand", 0x2030},
	{"pi", 0x03C0},
	{"plus", 0x002B},
	{"plusminus", 0x00B1},
	{"product", 0x220F},
	{"q", 0x0071},
	{"question", 0x003F},
	{"questiondown", 0x00BF},
	{"quotedbl", 0x0022},
	{"quotedblbase", 0x201E},
	{"quotedblleft", 0x201C},
	{"quotedblright", 0x201D},
	{"quoteleft", 0x2018},
	{"quoteright", 0x2019},
	{"quotesinglbase", 0x201A},
	{"quotesingle", 0x0027},
	{"r", 0x0072},
	{"radical", 0x221A},
	{"rcaron", 0x0159},
	{"registered", 0x00AE},
	{"ring", 0x02DA},
	{"s", 0x0073},
	{"sacute", 0x015B},
	{"scaron", 0x0161},
	{"scedilla", 0x015F},
	{"section", 0x00A7},
	{"semicolon", 0x003B},
	{"seven", 0x0037},
	{"sfthyphen", 0x00AD},
	{"sigma", 0x03C3},
	{"six", 0x0036},
	{"slash", 0x002F},
	{"space", 0x0020},
	{"sterling", 0x00A3},
	{"summation", 0x2211},
	{"t", 0x0074},
	{"tcaron", 0x0165},
	{"thorn", 0x00FE},
	{"three", 0x0033},
	{"threequarters", 0x00BE},
	{"threesuperior", 0x00B3},
	{"tilde", 0x02DC},
	{"trademark", 0x2122},
	{"two", 0x0032},
	{"twosuperior", 0x00B2},
	{"u", 0x0075},
	{"uacute", 0x00FA},
	{"ucircumflex", 0x00FB},
	{"udieresis", 0x00FC},
	{"ugrave", 0x00F9},
	{"uhungarumlaut", 0x0171},
	{"umacron", 0x016B},
	{"underscore", 0x005F},
	{"uring", 0x016F},
	{"v", 0x0076},
	{"w", 0x0077},
	{"x", 0x0078},
	{"y", 0x0079},
	{"yacute", 0x00FD},
	{"ydieresis", 0x00FF},
	{"yen", 0x00A5},
	{"z", 0x007A},
	{"zacute", 0x017A},
	{"zcaron", 0x017E},
	{"zdotaccent", 0x017C},
	{"zero", 0x0030}};

/* ---- las DejaVu (las pone pinta_fuentes) ---- */
static struct { const u8 *d; u32 n; stbtt_fontinfo f; int ok; } dejavu[6];
enum { SANS, SERIF, MONO, SANSN, SERIFN, MONON };

void pinta_fuentes(const u8 *sans, u32 n_sans, const u8 *serif, u32 n_serif, const u8 *mono, u32 n_mono,
		   const u8 *sansn, u32 n_sansn, const u8 *serifn, u32 n_serifn, const u8 *monon, u32 n_monon)
{
	const u8 *d[6] = {sans, serif, mono, sansn, serifn, monon};
	u32 n[6] = {n_sans, n_serif, n_mono, n_sansn, n_serifn, n_monon};
	int i;

	for (i = 0; i < 6; i++) {
		dejavu[i].d = d[i];
		dejavu[i].n = n[i];
		dejavu[i].ok = d[i] && stbtt_InitFont(&dejavu[i].f, d[i], 0);
	}
}

/* ---- la caché de cada documento ---- */
typedef struct {
	pobj *dic;
	pfuente *f;
} pcache;

void fuentes_suelta(pdoc *d)
{
	so_suelta(d->fuentes);
	d->fuentes = NULL;
	d->n_fuentes = 0;
}

u32 unicode_de_nombre(const char *n)
{
	int a = 0, b = (int)(sizeof nombres_letra / sizeof nombres_letra[0]) - 1;
	u32 v = 0;
	const char *p;

	while (a <= b) {
		int m = (a + b) / 2, c = strcmp(n, nombres_letra[m].n);

		if (!c)
			return nombres_letra[m].u;
		if (c < 0)
			b = m - 1;
		else
			a = m + 1;
	}
	/* uniXXXX y uXXXX[XX] */
	if (n[0] == 'u' && n[1] == 'n' && n[2] == 'i')
		p = n + 3;
	else if (n[0] == 'u' && n[1])
		p = n + 1;
	else
		p = NULL;
	if (p) {
		int k = 0;

		for (; *p && k < 6; p++, k++) {
			int c = *p;

			if (c >= '0' && c <= '9')
				v = v * 16 + (u32)(c - '0');
			else if (c >= 'A' && c <= 'F')
				v = v * 16 + (u32)(c - 'A' + 10);
			else
				break;
		}
		if (k >= 4 && (!*p || *p == '.' || *p == '_'))
			return v;
	}
	/* «a.sc», «f_i»: lo de antes del punto */
	{
		const char *pt = strchr(n, '.');
		char t[64];

		if (pt && pt > n && pt - n < 63) {
			memcpy(t, n, (size_t)(pt - n));
			t[pt - n] = 0;
			return unicode_de_nombre(t);
		}
	}
	/* «g12», «cid123», «Gxx» (de los que no dicen nada) */
	return 0;
}

/* ---- ToUnicode (bfchar y bfrange; codespacerange para los largos) ---- */
static void mete_tramo(pdoc *d, pfuente *f, u32 *cap, u32 desde, u32 hasta, u32 *u, int n, int bytes)
{
	ptramo *t;

	if (f->n_tu >= 200000)
		return;
	if (f->n_tu >= *cap) {
		ptramo *nuevo;

		*cap = *cap ? *cap * 2 : 64;
		nuevo = ar_pide(d, *cap * (u32)sizeof(ptramo));
		if (f->n_tu)
			memcpy(nuevo, f->tu, f->n_tu * sizeof(ptramo));
		f->tu = nuevo;
	}
	t = &f->tu[f->n_tu++];
	t->desde = desde;
	t->hasta = hasta;
	t->bytes = (u8)bytes;
	t->n = (u8)n;
	if (n <= 1)
		t->uni = n ? u[0] : 0;
	else {
		t->cadena = ar_pide(d, (u32)n * 4);
		memcpy(t->cadena, u, (size_t)n * 4);
	}
}

static u32 cadena_a_num(const u8 *s, u32 n)
{
	u32 v = 0, i;

	for (i = 0; i < n && i < 4; i++)
		v = v << 8 | s[i];
	return v;
}

/* UTF-16BE a letras */
static int utf16(const u8 *s, u32 n, u32 *u, int max)
{
	u32 i;
	int k = 0;

	for (i = 0; i + 1 < n && k < max; i += 2) {
		u32 c = (u32)s[i] << 8 | s[i + 1];

		if (c >= 0xD800 && c < 0xDC00 && i + 3 < n) {
			u32 c2 = (u32)s[i + 2] << 8 | s[i + 3];

			c = 0x10000 + ((c - 0xD800) << 10) + (c2 - 0xDC00);
			i += 2;
		}
		u[k++] = c;
	}
	return k;
}

static void lee_tounicode(pdoc *d, pfuente *f, pobj *flujo)
{
	u32 n, cap = 0;
	u8 *t = flujo_datos(d, flujo, &n, 0, NULL);
	plex l;
	ptok k;
	u8 a[8], b[8];
	u32 la, lb;
	const u8 *tras;

	if (!t)
		return;
	l.ini = l.p = t;
	l.fin = t + n;
	for (;;) {
		if (lex_token(&l, &k) == T_FIN)
			break;
		if (k.t != T_PALABRA)
			continue;
		if (tok_es(&k, "begincodespacerange")) {
			for (;;) {
				ptok x, y;

				if (lex_token(&l, &x) != T_HEX || lex_token(&l, &y) != T_HEX)
					break;
				if (x.n >= 1 && x.n <= 4)
					f->bytes_tu[x.n] = 1;
			}
		} else if (tok_es(&k, "beginbfchar")) {
			for (;;) {
				ptok x, y;
				u32 u[16];
				int m;

				if (lex_token(&l, &x) != T_HEX)
					break;
				if (lex_token(&l, &y) == T_HEX) {
					u8 bb[64];

					la = cadena_hex(x.p, l.fin, a, &tras) ;
					if (y.n > 64)
						continue;
					lb = cadena_hex(y.p, l.fin, bb, &tras);
					m = utf16(bb, lb, u, 16);
					if (la >= 1 && la <= 4)
						mete_tramo(d, f, &cap, cadena_a_num(a, la), cadena_a_num(a, la), u, m, (int)la);
				} else if (y.t == T_NOMBRE) {
					char nm[64];
					u32 nn = y.n < 63 ? y.n : 63;

					memcpy(nm, y.p, nn);
					nm[nn] = 0;
					u[0] = unicode_de_nombre(nm);
					la = cadena_hex(x.p, l.fin, a, &tras);
					if (u[0] && la >= 1 && la <= 4)
						mete_tramo(d, f, &cap, cadena_a_num(a, la), cadena_a_num(a, la), u, 1, (int)la);
				} else
					break;
			}
		} else if (tok_es(&k, "beginbfrange")) {
			for (;;) {
				ptok x, y, z;
				u32 desde, hasta;

				if (lex_token(&l, &x) != T_HEX || lex_token(&l, &y) != T_HEX)
					break;
				if (x.n > 8 || y.n > 8)
					break;
				la = cadena_hex(x.p, l.fin, a, &tras);
				lb = cadena_hex(y.p, l.fin, b, &tras);
				desde = cadena_a_num(a, la);
				hasta = cadena_a_num(b, lb);
				if (hasta < desde || hasta - desde > 65535 || la < 1 || la > 4)
					hasta = desde;
				lex_token(&l, &z);
				if (z.t == T_HEX) {
					u8 bb[64];
					u32 u[16];
					int m;

					if (z.n > 64)
						continue;
					lb = cadena_hex(z.p, l.fin, bb, &tras);
					m = utf16(bb, lb, u, 16);
					if (m == 1)
						mete_tramo(d, f, &cap, desde, hasta, u, 1, (int)la);
					else if (m > 1 && desde == hasta)
						mete_tramo(d, f, &cap, desde, hasta, u, m, (int)la);
					else if (m > 1) {
						/* (una cadena de varias que va creciendo en la última) */
						u32 c;

						for (c = desde; c <= hasta && c - desde < 256; c++) {
							mete_tramo(d, f, &cap, c, c, u, m, (int)la);
							u[m - 1]++;
						}
					}
				} else if (z.t == T_ABRE_A) {
					u32 c = desde;

					for (;;) {
						ptok w;
						u8 bb[64];
						u32 u[16];
						int m;

						if (lex_token(&l, &w) != T_HEX)
							break;
						if (w.n > 64)
							continue;
						lb = cadena_hex(w.p, l.fin, bb, &tras);
						m = utf16(bb, lb, u, 16);
						if (c <= hasta)
							mete_tramo(d, f, &cap, c, c, u, m, (int)la);
						c++;
					}
				} else
					break;
			}
		}
	}
	so_suelta(t);
}

/* ---- la codificación de las simples ---- */
static void codificacion(pdoc *d, pfuente *f, pobj *enc, int simbolica)
{
	const u16 *base = simbolica ? NULL : estandar;
	pobj *dif = NULL;
	int i;

	if (enc && enc->t == O_DICC) {
		pobj *b = dic_r(d, enc, "BaseEncoding");

		dif = dic_r(d, enc, "Differences");
		enc = b;
	}
	if (o_es_nombre(enc, "WinAnsiEncoding"))
		base = win_ansi;
	else if (o_es_nombre(enc, "MacRomanEncoding") || o_es_nombre(enc, "MacExpertEncoding"))
		base = mac_roman;
	else if (o_es_nombre(enc, "StandardEncoding"))
		base = estandar;
	else if (!enc && !simbolica && !base)
		base = estandar;
	for (i = 0; i < 256; i++)
		f->uni[i] = base ? base[i] : (u32)(i >= 32 && i < 127 ? i : 0);
	if (dif && dif->t == O_ARREGLO) {
		u32 k, c = 0;

		for (k = 0; k < dif->n; k++) {
			pobj *x = resuelve(d, dif->u.v[k]);

			if (x->t == O_ENT)
				c = (u32)x->u.i;
			else if (x->t == O_NOMBRE) {
				if (c < 256) {
					f->nombres[c] = (char *)x->u.s;
					f->uni[c] = unicode_de_nombre((const char *)x->u.s);
				}
				c++;
			}
		}
	}
}

/* ---- las tablas cmap de una TrueType ---- */
static void cmaps(pfuente *f, stbtt_fontinfo *tt)
{
	const u8 *data = tt->data;
	u32 cmap = stbtt__find_table(tt->data, (u32)tt->fontstart, "cmap"), n, i;

	if (!cmap)
		return;
	n = ttUSHORT(data + cmap + 2);
	for (i = 0; i < n && i < 64; i++) {
		u32 r = cmap + 4 + 8 * i, pid = ttUSHORT(data + r), eid = ttUSHORT(data + r + 2), off = cmap + ttULONG(data + r + 4);

		if (pid == 3 && eid == 1)
			f->cmap31 = off;
		else if (pid == 3 && eid == 0)
			f->cmap30 = off;
		else if (pid == 1 && eid == 0)
			f->cmap10 = off;
		else if (pid == 0 && !f->cmap31)
			f->cmap31 = off;
	}
}

/* ---- la TrueType del PDF, comprobada (viene de Internet: stb_truetype
 * se fía de lo que lee) ---- */
static u32 rd16(const u8 *d, u32 n, u32 o) { return o + 2 <= n ? (u32)d[o] << 8 | d[o + 1] : 0; }
static u32 rd32(const u8 *d, u32 n, u32 o) { return o + 4 <= n ? (u32)d[o] << 24 | (u32)d[o + 1] << 16 | (u32)d[o + 2] << 8 | d[o + 3] : 0; }

/* una tabla: su sitio y lo que mide (0 si no está o se sale) */
static u32 tabla(const u8 *d, u32 n, const char *nombre, u32 *largo)
{
	u32 nt = rd16(d, n, 4), i;

	*largo = 0;
	if (12 + 16 * nt > n)
		return 0;
	for (i = 0; i < nt; i++) {
		u32 r = 12 + 16 * i, off = rd32(d, n, r + 8), l = rd32(d, n, r + 12);

		if (memcmp(d + r, nombre, 4) == 0) {
			if (off > n || l > n - off)
				return 0;
			*largo = l;
			return off;
		}
	}
	return 0;
}

static int tt_valida(pfuente *f, const u8 *d, u32 n)
{
	u32 l, head, hhea, hmtx, maxp, loca, glyf, cmap, lh, lx, ll, lg, lm, lc, nl, ng, i, nt;

	if (n < 12)
		return 0;
	nt = rd16(d, n, 4);
	if (nt == 0 || 12 + 16 * nt > n)
		return 0;
	for (i = 0; i < nt; i++) {
		u32 off = rd32(d, n, 12 + 16 * i + 8), largo = rd32(d, n, 12 + 16 * i + 12);

		if (off > n || largo > n - off)
			return 0;
	}
	head = tabla(d, n, "head", &l);
	if (!head || l < 54)
		return 0;
	hhea = tabla(d, n, "hhea", &lh);
	hmtx = tabla(d, n, "hmtx", &lx);
	maxp = tabla(d, n, "maxp", &lm);
	loca = tabla(d, n, "loca", &ll);
	glyf = tabla(d, n, "glyf", &lg);
	cmap = tabla(d, n, "cmap", &lc);
	if (!hhea || lh < 36 || !hmtx || !maxp || lm < 6 || !loca || !glyf || !cmap || lc < 4)
		return 0;
	ng = rd16(d, n, maxp + 4);
	nl = rd16(d, n, hhea + 34);
	if (ng == 0 || nl == 0 || nl > ng)
		return 0;
	if ((u64)4 * nl + 2 * (u64)(ng - nl) > lx)
		return 0;
	f->loca_larga = rd16(d, n, head + 50);
	if (f->loca_larga > 1)
		return 0;
	if ((u64)(ng + 1) * (f->loca_larga ? 4 : 2) > ll)
		return 0;
	if (4 + 8 * rd16(d, n, cmap + 2) > lc)
		return 0;
	/* loca: en orden y dentro de glyf */
	{
		u32 antes = 0;

		for (i = 0; i <= ng; i++) {
			u32 v = f->loca_larga ? rd32(d, n, loca + 4 * i) : 2 * rd16(d, n, loca + 2 * i);

			if (v < antes || v > lg)
				return 0;
			antes = v;
		}
	}
	f->tt_d = d;
	f->tt_n = n;
	f->n_glifos = ng;
	f->loca = loca;
	f->glyf = glyf;
	f->glyf_n = lg;
	return 1;
}

static int glifo_bien(pfuente *f, u32 g, int hondo)
{
	const u8 *d = f->tt_d;
	u32 n = f->tt_n, a, b, p, fin;
	int nc;

	if (g >= f->n_glifos || hondo > 4)
		return 0;
	if (f->glifo_ok && f->glifo_ok[g])
		return f->glifo_ok[g] == 1;
	a = f->loca_larga ? rd32(d, n, f->loca + 4 * g) : 2 * rd16(d, n, f->loca + 2 * g);
	b = f->loca_larga ? rd32(d, n, f->loca + 4 * g + 4) : 2 * rd16(d, n, f->loca + 2 * g + 2);
	if (a == b)
		return 1;	/* (vacío) */
	if (b - a < 10)
		return 0;
	p = f->glyf + a;
	fin = f->glyf + b;
	nc = (int)(int16_t)rd16(d, n, p);
	if (nc >= 0) {
		u32 q = p + 10, puntos, ins, i, xs = 0, ys = 0, ultimo = 0;

		if (q + 2 * (u32)nc + 2 > fin)
			return 0;
		for (i = 0; i < (u32)nc; i++) {
			u32 e = rd16(d, n, q + 2 * i);

			if (i && e < ultimo)
				return 0;
			ultimo = e;
		}
		puntos = nc ? ultimo + 1 : 0;
		ins = rd16(d, n, q + 2 * (u32)nc);
		q += 2 * (u32)nc + 2 + ins;
		/* las banderas (con sus repeticiones) y lo que miden las x y las y */
		for (i = 0; i < puntos;) {
			u32 fl, rep = 1;

			if (q >= fin)
				return 0;
			fl = d[q++];
			if (fl & 8) {
				if (q >= fin)
					return 0;
				rep += d[q++];
			}
			if (i + rep > puntos)
				rep = puntos - i;	/* (stb para en los puntos) */
			xs += rep * ((fl & 2) ? 1 : (fl & 16) ? 0 : 2);
			ys += rep * ((fl & 4) ? 1 : (fl & 32) ? 0 : 2);
			i += rep;
		}
		if ((u64)q + xs + ys > fin)
			return 0;
		return 1;
	} else {
		u32 q = p + 10, fl;
		int k = 0;

		do {
			u32 sub;

			if (q + 4 > fin || ++k > 64)
				return 0;
			fl = rd16(d, n, q);
			sub = rd16(d, n, q + 2);
			q += 4 + ((fl & 1) ? 4 : 2);
			if (fl & 8)
				q += 2;
			else if (fl & 0x40)
				q += 4;
			else if (fl & 0x80)
				q += 8;
			if (q > fin)
				return 0;
			if (sub == g || !glifo_bien(f, sub, hondo + 1))
				return 0;
		} while (fl & 0x20);
		return 1;
	}
}

static int glifo_vale(pdoc *d, pfuente *f, u32 g)
{
	int r;

	if (!f->tt_d)
		return 1;	/* (las DejaVu) */
	if (g >= f->n_glifos)
		return 0;
	if (!f->glifo_ok) {
		f->glifo_ok = ar_pide(d, f->n_glifos);
	}
	if (f->glifo_ok[g])
		return f->glifo_ok[g] == 1;
	r = glifo_bien(f, g, 0);
	f->glifo_ok[g] = (u8)(r ? 1 : 2);
	return r;
}

/* el glifo del carácter c en la subtabla cmap de «off» (formatos 0, 4, 6 y 12), mirando los límites */
static int cmap_busca(const u8 *d, u32 n, u32 off, u32 c)
{
	u32 fmt = rd16(d, n, off);

	if (!off || off >= n)
		return 0;
	if (fmt == 0) {
		return c < 256 && off + 6 + c < n ? d[off + 6 + c] : 0;
	}
	if (fmt == 6) {
		u32 primero = rd16(d, n, off + 6), cuantos = rd16(d, n, off + 8);

		if (c < primero || c >= primero + cuantos)
			return 0;
		return (int)rd16(d, n, off + 10 + 2 * (c - primero));
	}
	if (fmt == 4) {
		u32 seg2 = rd16(d, n, off + 6), segs = seg2 / 2, i;
		u32 fines = off + 14, inis = fines + seg2 + 2, deltas = inis + seg2, rangos = deltas + seg2;

		if (c > 0xFFFF || rangos + seg2 > n)
			return 0;
		for (i = 0; i < segs; i++) {
			u32 fin_ = rd16(d, n, fines + 2 * i), ini = rd16(d, n, inis + 2 * i);

			if (c > fin_)
				continue;
			if (c < ini)
				return 0;
			{
				u32 delta = rd16(d, n, deltas + 2 * i), ro = rd16(d, n, rangos + 2 * i), g;

				if (!ro)
					return (int)((c + delta) & 0xFFFF);
				g = rd16(d, n, rangos + 2 * i + ro + 2 * (c - ini));
				return g ? (int)((g + delta) & 0xFFFF) : 0;
			}
		}
		return 0;
	}
	if (fmt == 12) {
		u32 grupos = rd32(d, n, off + 12), i;

		if (grupos > (n - off) / 12)
			return 0;
		for (i = 0; i < grupos; i++) {
			u32 a = rd32(d, n, off + 16 + 12 * i), b = rd32(d, n, off + 20 + 12 * i), g = rd32(d, n, off + 24 + 12 * i);

			if (c >= a && c <= b)
				return (int)(g + (c - a));
		}
	}
	return 0;
}

static int glifo_con(pfuente *f, u32 tabla_, u32 c)
{
	if (!tabla_ || !f->tt_d)
		return 0;
	return cmap_busca(f->tt_d, f->tt_n, tabla_, c);
}

/* ¿la letra del PDF se puede usar? (de lo que trae: TrueType u OpenType) */
static void programa(pdoc *d, pfuente *f, pobj *desc)
{
	pobj *ff = dic_r(d, desc, "FontFile2"), *f3 = dic_r(d, desc, "FontFile3");
	u8 *datos;
	u32 n;
	stbtt_fontinfo *tt;

	if ((!ff || ff->t != O_FLUJO) && f3 && f3->t == O_FLUJO && o_es_nombre(dic_r(d, f3, "Subtype"), "OpenType"))
		ff = f3;
	if (!ff || ff->t != O_FLUJO)
		return;
	datos = flujo_datos(d, ff, &n, 0, NULL);
	if (!datos)
		return;
	if (n < 12 || n > 32u * 1024 * 1024) {
		so_suelta(datos);
		return;
	}
	{
		u8 *copia = ar_pide(d, n + 16);

		memcpy(copia, datos, n);
		so_suelta(datos);
		datos = copia;
	}
	tt = ar_pide(d, sizeof(stbtt_fontinfo));
	/* solo las TrueType (glyf), comprobadas; las CFF, con la DejaVu */
	if (!tt_valida(f, datos, n) || !stbtt_InitFont(tt, datos, 0)) {
		f->tt_d = NULL;
		return;
	}
	f->tt = tt;
	f->propia = 1;
	{
		u32 head = stbtt__find_table(datos, 0, "head");
		u32 upem = head && head + 20 <= n ? ttUSHORT(datos + head + 18) : 1000;

		f->em = 1.0 / (upem >= 16 && upem <= 16384 ? upem : 1000);
	}
	cmaps(f, tt);
}

static int elige_respaldo(pdoc *d, pobj *dic, pobj *desc)
{
	pobj *bn = dic_r(d, dic, "BaseFont");
	const char *n = bn && bn->t == O_NOMBRE ? (const char *)bn->u.s : "";
	int flags = (int)o_num(dic_r(d, desc, "Flags"), 0), negrita = 0, k = SANS;
	char t[128];
	u32 i;

	for (i = 0; n[i] && i < 127; i++)
		t[i] = (char)(n[i] >= 'a' && n[i] <= 'z' ? n[i] - 32 : n[i]);
	t[i] = 0;
	if (strstr(t, "BOLD") || strstr(t, "BLACK") || strstr(t, "HEAVY") || o_num(dic_r(d, desc, "FontWeight"), 400) >= 600)
		negrita = 1;
	if (strstr(t, "COURIER") || strstr(t, "MONO") || (flags & 1))
		k = MONO;
	else if (strstr(t, "TIMES") || (strstr(t, "SERIF") && !strstr(t, "SANS")) || strstr(t, "ROMAN") || strstr(t, "GEORGIA") ||
		 strstr(t, "GARAMOND") || strstr(t, "BOOK") || strstr(t, "PALATINO") || strstr(t, "CAMBRIA") || strstr(t, "CMR") ||
		 strstr(t, "MINION") || ((flags & 2) && !strstr(t, "ARIAL") && !strstr(t, "HELV")))
		k = SERIF;
	return negrita ? k + 3 : k;
}

pfuente *fuente_de(pdoc *d, pobj *dic)
{
	pcache *c = d->fuentes;
	pfuente *f;
	pobj *st, *desc, *tu;
	u32 i;
	int simbolica;

	if (!dic || dic->t != O_DICC)
		return NULL;
	for (i = 0; i < d->n_fuentes; i++)
		if (c[i].dic == dic)
			return c[i].f;
	if (d->n_fuentes >= 4096)
		return NULL;
	if (d->n_fuentes % 16 == 0) {
		d->fuentes = repide(d->fuentes, (d->n_fuentes + 16) * sizeof(pcache));
		c = d->fuentes;
	}
	f = ar_pide(d, sizeof(pfuente));
	c[d->n_fuentes].dic = dic;
	c[d->n_fuentes].f = f;
	d->n_fuentes++;
	f->dic = dic;
	f->doc = d;
	f->em = 1.0 / 1000;
	f->ancho_def = 1000;
	st = dic_r(d, dic, "Subtype");
	tu = dic_r(d, dic, "ToUnicode");
	if (o_es_nombre(st, "Type0")) {
		pobj *ds = dic_r(d, dic, "DescendantFonts"), *cid, *w, *map;

		f->compuesta = 1;
		cid = ds && ds->t == O_ARREGLO && ds->n ? resuelve(d, ds->u.v[0]) : NULL;
		desc = cid ? dic_r(d, cid, "FontDescriptor") : NULL;
		f->ancho_def = o_num(dic_r(d, cid, "DW"), 1000);
		w = cid ? dic_r(d, cid, "W") : NULL;
		if (w && w->t == O_ARREGLO) {
			u32 k = 0, cap = 0;

			while (k < w->n && f->n_anchos_cid < 100000) {
				pobj *a = resuelve(d, w->u.v[k]);
				pobj *b = k + 1 < w->n ? resuelve(d, w->u.v[k + 1]) : NULL;

				if (!o_es_num(a) || !b)
					break;
				if (b->t == O_ARREGLO) {
					u32 j, c0 = (u32)o_num(a, 0);

					for (j = 0; j < b->n && f->n_anchos_cid < 100000; j++) {
						if (f->n_anchos_cid >= cap) {
							void *nuevo;

							cap = cap ? cap * 2 : 64;
							nuevo = ar_pide(d, cap * (u32)sizeof(f->anchos_cid[0]));
							if (f->n_anchos_cid)
								memcpy(nuevo, f->anchos_cid, f->n_anchos_cid * sizeof(f->anchos_cid[0]));
							f->anchos_cid = nuevo;
						}
						f->anchos_cid[f->n_anchos_cid].desde = c0 + j;
						f->anchos_cid[f->n_anchos_cid].hasta = c0 + j;
						f->anchos_cid[f->n_anchos_cid].w = o_num(resuelve(d, b->u.v[j]), f->ancho_def);
						f->n_anchos_cid++;
					}
					k += 2;
				} else {
					pobj *x = k + 2 < w->n ? resuelve(d, w->u.v[k + 2]) : NULL;

					if (!o_es_num(b) || !o_es_num(x))
						break;
					if (f->n_anchos_cid >= cap) {
						void *nuevo;

						cap = cap ? cap * 2 : 64;
						nuevo = ar_pide(d, cap * (u32)sizeof(f->anchos_cid[0]));
						if (f->n_anchos_cid)
							memcpy(nuevo, f->anchos_cid, f->n_anchos_cid * sizeof(f->anchos_cid[0]));
						f->anchos_cid = nuevo;
					}
					f->anchos_cid[f->n_anchos_cid].desde = (u32)o_num(a, 0);
					f->anchos_cid[f->n_anchos_cid].hasta = (u32)o_num(b, 0);
					f->anchos_cid[f->n_anchos_cid].w = o_num(x, f->ancho_def);
					f->n_anchos_cid++;
					k += 3;
				}
			}
		}
		if (desc)
			programa(d, f, desc);
		map = cid ? dic_r(d, cid, "CIDToGIDMap") : NULL;
		if (map && map->t == O_FLUJO) {
			u32 n;
			u8 *m = flujo_datos(d, map, &n, 0, NULL);

			if (m) {
				f->n_cid_gid = n / 2;
				f->cid_gid = ar_pide(d, f->n_cid_gid * 2 + 2);
				for (i = 0; i < f->n_cid_gid; i++)
					f->cid_gid[i] = (u16)(m[2 * i] << 8 | m[2 * i + 1]);
				so_suelta(m);
			}
		}
		f->respaldo = elige_respaldo(d, cid ? cid : dic, desc);
	} else {
		pobj *w = dic_r(d, dic, "Widths");
		int primera = (int)o_num(dic_r(d, dic, "FirstChar"), 0);

		desc = dic_r(d, dic, "FontDescriptor");
		simbolica = ((int)o_num(dic_r(d, desc, "Flags"), 0) & 4) != 0;
		f->simbolica = simbolica;
		f->tipo3 = o_es_nombre(st, "Type3");
		codificacion(d, f, dic_r(d, dic, "Encoding"), simbolica && !dic_r(d, dic, "Encoding"));
		if (simbolica && !dic_r(d, dic, "Encoding"))
			for (i = 0; i < 256; i++)
				if (!f->uni[i] && i >= 32)
					f->uni[i] = i;
		{
			double mw = o_num(dic_r(d, desc, "MissingWidth"), -1);

			for (i = 0; i < 256; i++)
				f->ancho[i] = mw;
		}
		if (w && w->t == O_ARREGLO)
			for (i = 0; i < w->n && primera + (int)i < 256; i++)
				if (primera + (int)i >= 0)
					f->ancho[primera + i] = o_num(resuelve(d, w->u.v[i]), 0);
		if (f->tipo3) {
			pobj *fm = dic_r(d, dic, "FontMatrix");

			f->fm[0] = f->fm[3] = 0.001;
			if (fm && fm->t == O_ARREGLO && fm->n >= 6)
				for (i = 0; i < 6; i++)
					f->fm[i] = o_num(resuelve(d, fm->u.v[i]), 0);
			f->procs = dic_r(d, dic, "CharProcs");
			f->recursos3 = dic_r(d, dic, "Resources");
			/* (los anchos de una Type3 van en su espacio: a milésimas) */
			for (i = 0; i < 256; i++)
				if (f->ancho[i] > 0)
					f->ancho[i] = f->ancho[i] * f->fm[0] * 1000;
		} else if (desc)
			programa(d, f, desc);
		f->respaldo = elige_respaldo(d, dic, desc);
	}
	if (tu && tu->t == O_FLUJO)
		lee_tounicode(d, f, tu);
	return f;
}

int fuente_codigo(pfuente *f, const u8 *s, u32 n, u32 *codigo)
{
	if (!n)
		return 0;
	if (f && f->compuesta) {
		if (n >= 2) {
			*codigo = (u32)s[0] << 8 | s[1];
			return 2;
		}
		*codigo = s[0];
		return 1;
	}
	*codigo = s[0];
	return 1;
}

int fuente_unicode(pfuente *f, u32 codigo, u32 *u, int max)
{
	u32 a, b;

	if (!f)
		return 0;
	if (f->n_tu) {
		/* (los tramos no van en orden: se miran todos; los últimos ganan) */
		int i;

		for (i = (int)f->n_tu - 1; i >= 0; i--) {
			ptramo *t = &f->tu[i];

			if (codigo >= t->desde && codigo <= t->hasta) {
				if (t->n <= 1) {
					if (!t->n)
						return 0;
					u[0] = t->uni + (codigo - t->desde);
					return 1;
				}
				a = t->n < (u32)max ? t->n : (u32)max;
				for (b = 0; b < a; b++)
					u[b] = t->cadena[b];
				return (int)a;
			}
		}
	}
	if (!f->compuesta && codigo < 256 && f->uni[codigo]) {
		u[0] = f->uni[codigo];
		return 1;
	}
	return 0;
}

static u32 upem_de(stbtt_fontinfo *tt)
{
	u32 head = stbtt__find_table(tt->data, (u32)tt->fontstart, "head");
	u32 u = head ? ttUSHORT(tt->data + head + 18) : 2048;

	return u >= 16 && u <= 16384 ? u : 2048;
}

int glifo(pfuente *f, u32 codigo);

double fuente_ancho(pfuente *f, u32 codigo)
{
	u32 i;

	if (!f)
		return 500;
	if (f->compuesta) {
		for (i = 0; i < f->n_anchos_cid; i++)
			if (codigo >= f->anchos_cid[i].desde && codigo <= f->anchos_cid[i].hasta)
				return f->anchos_cid[i].w;
		return f->ancho_def;
	}
	if (codigo < 256 && f->ancho[codigo] >= 0)
		return f->ancho[codigo];
	/* sin /Widths (las 14 de siempre): el de su letra o el de la DejaVu */
	{
		stbtt_fontinfo *tt = f->tt ? f->tt : (dejavu[f->respaldo].ok ? &dejavu[f->respaldo].f : NULL);
		int g, av, lsb;
		u32 u = codigo < 256 ? f->uni[codigo] : 0;

		if (!tt)
			return 500;
		if (f->tt) {
			g = glifo(f, codigo);
			if (g && !glifo_vale(f->doc, f, (u32)g))
				g = 0;
		} else
			g = stbtt_FindGlyphIndex(tt, (int)(u ? u : codigo));
		if (!g)
			return u == ' ' ? 250 : 500;
		stbtt_GetGlyphHMetrics(tt, g, &av, &lsb);
		return av * (f->tt ? f->em : 1.0 / upem_de(tt)) * 1000;
	}
}

/* el glifo de la letra en su TrueType */
int glifo(pfuente *f, u32 codigo)
{
	stbtt_fontinfo *tt = f->tt;
	int g = 0;

	if (f->compuesta) {
		if (f->cid_gid)
			return codigo < f->n_cid_gid ? f->cid_gid[codigo] : 0;
		return (int)codigo;	/* Identity (o una OpenType: el CID es el glifo) */
	}
	if (f->cmap30) {
		g = glifo_con(f, f->cmap30, 0xF000 + codigo);
		if (!g)
			g = glifo_con(f, f->cmap30, codigo);
	}
	if (!g && f->cmap31 && f->uni[codigo])
		g = glifo_con(f, f->cmap31, f->uni[codigo]);
	if (!g && f->cmap10)
		g = glifo_con(f, f->cmap10, codigo);
	if (!g && f->cmap31)
		g = glifo_con(f, f->cmap31, codigo);
	if (!g && f->nombres[codigo]) {
		/* (por su nombre: no se buscan en la tabla post) */
	}
	return g;
}

int fuente_dibujo(pfuente *f, u32 codigo, ptrazo *t, double *escala_h)
{
	stbtt_fontinfo *tt;
	stbtt_vertex *v = NULL;
	int g = 0, n, i;
	double em;
	int abierto = 0;

	*escala_h = 1;
	if (!f)
		return 0;
	if (f->tt) {
		tt = f->tt;
		g = glifo(f, codigo);
		em = f->em;
		if (g && !glifo_vale(f->doc, f, (u32)g))
			g = 0;
	}
	if (!g) {
		/* la DejaVu, con el ancho de la suya */
		u32 u[4];
		int k = fuente_unicode(f, codigo, u, 4);
		int av, lsb;
		u32 upem;

		if (!dejavu[f->respaldo].ok)
			return 0;
		tt = &dejavu[f->respaldo].f;
		if (k <= 0) {
			if (f->compuesta)
				return 0;
			u[0] = codigo;
		}
		if (u[0] == ' ' || u[0] == 0xA0)
			return 0;
		g = stbtt_FindGlyphIndex(tt, (int)u[0]);
		if (!g)
			return 0;
		upem = upem_de(tt);
		em = 1.0 / upem;
		stbtt_GetGlyphHMetrics(tt, g, &av, &lsb);
		if (av > 0) {
			double suyo = fuente_ancho(f, codigo) / 1000, mio = av * em;

			if (suyo > 0 && mio > 0) {
				*escala_h = suyo / mio;
				if (*escala_h < 0.5)
					*escala_h = 0.5;
				if (*escala_h > 1.6)
					*escala_h = 1.6;
			}
		}
	}
	n = stbtt_GetGlyphShape(tt, g, &v);
	if (n <= 0)
		return 0;
	{
		double cx = 0, cy = 0;

		for (i = 0; i < n; i++) {
			double x = v[i].x * em, y = v[i].y * em;

			switch (v[i].type) {
			case STBTT_vmove:
				if (abierto)
					t->cierra(t->u);
				t->mueve(t->u, x, y);
				abierto = 1;
				break;
			case STBTT_vline:
				t->linea(t->u, x, y);
				break;
			case STBTT_vcurve: {
				/* (cuadrática a cúbica) */
				double qx = v[i].cx * em, qy = v[i].cy * em;

				t->curva(t->u, cx + 2.0 / 3 * (qx - cx), cy + 2.0 / 3 * (qy - cy), x + 2.0 / 3 * (qx - x),
					 y + 2.0 / 3 * (qy - y), x, y);
				break;
			}
			case STBTT_vcubic:
				t->curva(t->u, v[i].cx * em, v[i].cy * em, v[i].cx1 * em, v[i].cy1 * em, x, y);
				break;
			}
			cx = x;
			cy = y;
		}
	}
	if (abierto)
		t->cierra(t->u);
	stbtt_FreeShape(tt, v);
	return 1;
}
