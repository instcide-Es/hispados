; nombres.inc - Lector: las tablas para pasar el texto de un PDF a la página 437.
; Lo hace programas/lector/haznombres.py: no se toca a mano.
; Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.

lat_437:                                ; Latin-1, de A0 a FF
        db 0x20, 0xAD, 0x9B, 0x9C, 0x3F, 0x9D, 0x7C, 0x15, 0x22, 0x0E, 0xA6, 0xAE
        db 0xAA, 0x2D, 0x0F, 0x2D, 0xF8, 0xF1, 0xFD, 0x33, 0x27, 0xE6, 0x14, 0xFA
        db 0x2C, 0x31, 0xA7, 0xAF, 0xAC, 0xAB, 0x3F, 0xA8, 0x41, 0x41, 0x41, 0x41
        db 0x8E, 0x8F, 0x92, 0x80, 0x45, 0x90, 0x45, 0x45, 0x49, 0x49, 0x49, 0x49
        db 0x44, 0xA5, 0x4F, 0x4F, 0x4F, 0x4F, 0x99, 0x78, 0x4F, 0x55, 0x55, 0x55
        db 0x9A, 0x59, 0x3F, 0xE1, 0x85, 0xA0, 0x83, 0x61, 0x84, 0x86, 0x91, 0x87
        db 0x8A, 0x82, 0x88, 0x89, 0x8D, 0xA1, 0x8C, 0x8B, 0x64, 0xA4, 0x95, 0xA2
        db 0x93, 0x6F, 0x94, 0xF6, 0x6F, 0x97, 0xA3, 0x96, 0x81, 0x79, 0x3F, 0x98
uni_tab:                                ; dw Unicode, db 437; acaba en 0
        dw 0x0100
        db 0x41
        dw 0x0101
        db 0x61
        dw 0x0102
        db 0x41
        dw 0x0103
        db 0x61
        dw 0x0104
        db 0x41
        dw 0x0105
        db 0x61
        dw 0x0106
        db 0x43
        dw 0x0107
        db 0x63
        dw 0x0108
        db 0x43
        dw 0x0109
        db 0x63
        dw 0x010A
        db 0x43
        dw 0x010B
        db 0x63
        dw 0x010C
        db 0x43
        dw 0x010D
        db 0x63
        dw 0x010E
        db 0x44
        dw 0x010F
        db 0x64
        dw 0x0112
        db 0x45
        dw 0x0113
        db 0x65
        dw 0x0114
        db 0x45
        dw 0x0115
        db 0x65
        dw 0x0116
        db 0x45
        dw 0x0117
        db 0x65
        dw 0x0118
        db 0x45
        dw 0x0119
        db 0x65
        dw 0x011A
        db 0x45
        dw 0x011B
        db 0x65
        dw 0x011C
        db 0x47
        dw 0x011D
        db 0x67
        dw 0x011E
        db 0x47
        dw 0x011F
        db 0x67
        dw 0x0120
        db 0x47
        dw 0x0121
        db 0x67
        dw 0x0122
        db 0x47
        dw 0x0123
        db 0x67
        dw 0x0124
        db 0x48
        dw 0x0125
        db 0x68
        dw 0x0128
        db 0x49
        dw 0x0129
        db 0x69
        dw 0x012A
        db 0x49
        dw 0x012B
        db 0x69
        dw 0x012C
        db 0x49
        dw 0x012D
        db 0x69
        dw 0x012E
        db 0x49
        dw 0x012F
        db 0x69
        dw 0x0130
        db 0x49
        dw 0x0131
        db 0x69
        dw 0x0134
        db 0x4A
        dw 0x0135
        db 0x6A
        dw 0x0136
        db 0x4B
        dw 0x0137
        db 0x6B
        dw 0x0139
        db 0x4C
        dw 0x013A
        db 0x6C
        dw 0x013B
        db 0x4C
        dw 0x013C
        db 0x6C
        dw 0x013D
        db 0x4C
        dw 0x013E
        db 0x6C
        dw 0x0141
        db 0x4C
        dw 0x0142
        db 0x6C
        dw 0x0143
        db 0x4E
        dw 0x0144
        db 0x6E
        dw 0x0145
        db 0x4E
        dw 0x0146
        db 0x6E
        dw 0x0147
        db 0x4E
        dw 0x0148
        db 0x6E
        dw 0x014C
        db 0x4F
        dw 0x014D
        db 0x6F
        dw 0x014E
        db 0x4F
        dw 0x014F
        db 0x6F
        dw 0x0150
        db 0x4F
        dw 0x0151
        db 0x6F
        dw 0x0152
        db 0x08
        dw 0x0153
        db 0x0B
        dw 0x0154
        db 0x52
        dw 0x0155
        db 0x72
        dw 0x0156
        db 0x52
        dw 0x0157
        db 0x72
        dw 0x0158
        db 0x52
        dw 0x0159
        db 0x72
        dw 0x015A
        db 0x53
        dw 0x015B
        db 0x73
        dw 0x015C
        db 0x53
        dw 0x015D
        db 0x73
        dw 0x015E
        db 0x53
        dw 0x015F
        db 0x73
        dw 0x0160
        db 0x53
        dw 0x0161
        db 0x73
        dw 0x0162
        db 0x54
        dw 0x0163
        db 0x74
        dw 0x0164
        db 0x54
        dw 0x0165
        db 0x74
        dw 0x0168
        db 0x55
        dw 0x0169
        db 0x75
        dw 0x016A
        db 0x55
        dw 0x016B
        db 0x75
        dw 0x016C
        db 0x55
        dw 0x016D
        db 0x75
        dw 0x016E
        db 0x55
        dw 0x016F
        db 0x75
        dw 0x0170
        db 0x55
        dw 0x0171
        db 0x75
        dw 0x0172
        db 0x55
        dw 0x0173
        db 0x75
        dw 0x0174
        db 0x57
        dw 0x0175
        db 0x77
        dw 0x0176
        db 0x59
        dw 0x0177
        db 0x79
        dw 0x0178
        db 0x59
        dw 0x0179
        db 0x5A
        dw 0x017A
        db 0x7A
        dw 0x017B
        db 0x5A
        dw 0x017C
        db 0x7A
        dw 0x017D
        db 0x5A
        dw 0x017E
        db 0x7A
        dw 0x017F
        db 0x73
        dw 0x0192
        db 0x9F
        dw 0x02C6
        db 0x5E
        dw 0x02C7
        db 0x5E
        dw 0x02D8
        db 0x7E
        dw 0x02D9
        db 0x2E
        dw 0x02DA
        db 0xF8
        dw 0x02DB
        db 0x2C
        dw 0x02DC
        db 0x7E
        dw 0x02DD
        db 0x22
        dw 0x0393
        db 0xE2
        dw 0x0398
        db 0xE9
        dw 0x03A3
        db 0xE4
        dw 0x03A6
        db 0xE8
        dw 0x03A9
        db 0xEA
        dw 0x03B1
        db 0xE0
        dw 0x03B2
        db 0xE1
        dw 0x03B4
        db 0xEB
        dw 0x03B5
        db 0xEE
        dw 0x03BC
        db 0xE6
        dw 0x03C0
        db 0xE3
        dw 0x03C3
        db 0xE5
        dw 0x03C4
        db 0xE7
        dw 0x03C6
        db 0xED
        dw 0x2002
        db 0x20
        dw 0x2003
        db 0x20
        dw 0x2004
        db 0x20
        dw 0x2005
        db 0x20
        dw 0x2006
        db 0x20
        dw 0x2007
        db 0x20
        dw 0x2008
        db 0x20
        dw 0x2009
        db 0x20
        dw 0x200A
        db 0x20
        dw 0x2010
        db 0x2D
        dw 0x2011
        db 0x2D
        dw 0x2012
        db 0x2D
        dw 0x2013
        db 0x2D
        dw 0x2014
        db 0x2D
        dw 0x2015
        db 0x2D
        dw 0x2018
        db 0x27
        dw 0x2019
        db 0x27
        dw 0x201A
        db 0x2C
        dw 0x201B
        db 0x27
        dw 0x201C
        db 0x22
        dw 0x201D
        db 0x22
        dw 0x201E
        db 0x22
        dw 0x2020
        db 0x2B
        dw 0x2021
        db 0x2B
        dw 0x2022
        db 0xF9
        dw 0x2026
        db 0x07
        dw 0x202F
        db 0x20
        dw 0x2030
        db 0x25
        dw 0x2032
        db 0x27
        dw 0x2033
        db 0x22
        dw 0x2039
        db 0x3C
        dw 0x203A
        db 0x3E
        dw 0x2043
        db 0x2D
        dw 0x2044
        db 0x2F
        dw 0x207F
        db 0xFC
        dw 0x20A7
        db 0x9E
        dw 0x20AC
        db 0x06
        dw 0x2122
        db 0x0C
        dw 0x2190
        db 0x3C
        dw 0x2192
        db 0x3E
        dw 0x2212
        db 0x2D
        dw 0x2215
        db 0x2F
        dw 0x2217
        db 0x2A
        dw 0x2219
        db 0xF9
        dw 0x221A
        db 0xFB
        dw 0x221E
        db 0xEC
        dw 0x2229
        db 0xEF
        dw 0x2248
        db 0xF7
        dw 0x2261
        db 0xF0
        dw 0x2264
        db 0xF3
        dw 0x2265
        db 0xF2
        dw 0x2310
        db 0xA9
        dw 0x2320
        db 0xF4
        dw 0x2321
        db 0xF5
        dw 0x2500
        db 0xC4
        dw 0x2502
        db 0xB3
        dw 0x250C
        db 0xDA
        dw 0x2510
        db 0xBF
        dw 0x2514
        db 0xC0
        dw 0x2518
        db 0xD9
        dw 0x251C
        db 0xC3
        dw 0x2524
        db 0xB4
        dw 0x252C
        db 0xC2
        dw 0x2534
        db 0xC1
        dw 0x253C
        db 0xC5
        dw 0x2550
        db 0xCD
        dw 0x2551
        db 0xBA
        dw 0x2552
        db 0xD5
        dw 0x2553
        db 0xD6
        dw 0x2554
        db 0xC9
        dw 0x2555
        db 0xB8
        dw 0x2556
        db 0xB7
        dw 0x2557
        db 0xBB
        dw 0x2558
        db 0xD4
        dw 0x2559
        db 0xD3
        dw 0x255A
        db 0xC8
        dw 0x255B
        db 0xBE
        dw 0x255C
        db 0xBD
        dw 0x255D
        db 0xBC
        dw 0x255E
        db 0xC6
        dw 0x255F
        db 0xC7
        dw 0x2560
        db 0xCC
        dw 0x2561
        db 0xB5
        dw 0x2562
        db 0xB6
        dw 0x2563
        db 0xB9
        dw 0x2564
        db 0xD1
        dw 0x2565
        db 0xD2
        dw 0x2566
        db 0xCB
        dw 0x2567
        db 0xCF
        dw 0x2568
        db 0xD0
        dw 0x2569
        db 0xCA
        dw 0x256A
        db 0xD8
        dw 0x256B
        db 0xD7
        dw 0x256C
        db 0xCE
        dw 0x2580
        db 0xDF
        dw 0x2584
        db 0xDC
        dw 0x2588
        db 0xDB
        dw 0x258C
        db 0xDD
        dw 0x2590
        db 0xDE
        dw 0x2591
        db 0xB0
        dw 0x2592
        db 0xB1
        dw 0x2593
        db 0xB2
        dw 0x25A0
        db 0xFE
        dw 0x25AA
        db 0xFE
        dw 0x25CF
        db 0xF9
        dw 0x25E6
        db 0xF9
        dw 0x2713
        db 0x78
        dw 0x2714
        db 0x78
        dw 0x3000
        db 0x20
        dw 0xFB00
        db 0x03
        dw 0xFB01
        db 0x01
        dw 0xFB02
        db 0x02
        dw 0xFB03
        db 0x04
        dw 0xFB04
        db 0x05
        dw 0
win_tab:                                ; WinAnsi, de 80 a 9F
        dw 0x20AC, 0x0000, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021
        dw 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x0000, 0x017D, 0x0000
        dw 0x0000, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014
        dw 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x0000, 0x017E, 0x0178
mac_tab:                                ; MacRoman, de 80 a FF
        dw 0x00C4, 0x00C5, 0x00C7, 0x00C9, 0x00D1, 0x00D6, 0x00DC, 0x00E1
        dw 0x00E0, 0x00E2, 0x00E4, 0x00E3, 0x00E5, 0x00E7, 0x00E9, 0x00E8
        dw 0x00EA, 0x00EB, 0x00ED, 0x00EC, 0x00EE, 0x00EF, 0x00F1, 0x00F3
        dw 0x00F2, 0x00F4, 0x00F6, 0x00F5, 0x00FA, 0x00F9, 0x00FB, 0x00FC
        dw 0x2020, 0x00B0, 0x00A2, 0x00A3, 0x00A7, 0x2022, 0x00B6, 0x00DF
        dw 0x00AE, 0x00A9, 0x2122, 0x00B4, 0x00A8, 0x2260, 0x00C6, 0x00D8
        dw 0x221E, 0x00B1, 0x2264, 0x2265, 0x00A5, 0x00B5, 0x2202, 0x2211
        dw 0x220F, 0x03C0, 0x222B, 0x00AA, 0x00BA, 0x03A9, 0x00E6, 0x00F8
        dw 0x00BF, 0x00A1, 0x00AC, 0x221A, 0x0192, 0x2248, 0x2206, 0x00AB
        dw 0x00BB, 0x2026, 0x00A0, 0x00C0, 0x00C3, 0x00D5, 0x0152, 0x0153
        dw 0x2013, 0x2014, 0x201C, 0x201D, 0x2018, 0x2019, 0x00F7, 0x25CA
        dw 0x00FF, 0x0178, 0x2044, 0x20AC, 0x2039, 0x203A, 0xFB01, 0xFB02
        dw 0x2021, 0x00B7, 0x201A, 0x201E, 0x2030, 0x00C2, 0x00CA, 0x00C1
        dw 0x00CB, 0x00C8, 0x00CD, 0x00CE, 0x00CF, 0x00CC, 0x00D3, 0x00D4
        dw 0xF8FF, 0x00D2, 0x00DA, 0x00DB, 0x00D9, 0x0131, 0x02C6, 0x02DC
        dw 0x00AF, 0x02D8, 0x02D9, 0x02DA, 0x00B8, 0x02DD, 0x02DB, 0x02C7
std_tab:                                ; Standard de Adobe, de 20 a FF
        dw 0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x2019
        dw 0x0028, 0x0029, 0x002A, 0x002B, 0x002C, 0x002D, 0x002E, 0x002F
        dw 0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037
        dw 0x0038, 0x0039, 0x003A, 0x003B, 0x003C, 0x003D, 0x003E, 0x003F
        dw 0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047
        dw 0x0048, 0x0049, 0x004A, 0x004B, 0x004C, 0x004D, 0x004E, 0x004F
        dw 0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057
        dw 0x0058, 0x0059, 0x005A, 0x005B, 0x005C, 0x005D, 0x005E, 0x005F
        dw 0x2018, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067
        dw 0x0068, 0x0069, 0x006A, 0x006B, 0x006C, 0x006D, 0x006E, 0x006F
        dw 0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077
        dw 0x0078, 0x0079, 0x007A, 0x007B, 0x007C, 0x007D, 0x007E, 0x0000
        dw 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
        dw 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
        dw 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
        dw 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
        dw 0x0000, 0x00A1, 0x00A2, 0x00A3, 0x2044, 0x00A5, 0x0192, 0x00A7
        dw 0x00A4, 0x0027, 0x201C, 0x00AB, 0x2039, 0x203A, 0xFB01, 0xFB02
        dw 0x0000, 0x2013, 0x2020, 0x2021, 0x00B7, 0x0000, 0x00B6, 0x2022
        dw 0x201A, 0x201E, 0x201D, 0x00BB, 0x2026, 0x2030, 0x0000, 0x00BF
        dw 0x0000, 0x0060, 0x00B4, 0x02C6, 0x02DC, 0x00AF, 0x02D8, 0x02D9
        dw 0x00A8, 0x0000, 0x02DA, 0x00B8, 0x0000, 0x02DD, 0x02DB, 0x02C7
        dw 0x2014, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
        dw 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
        dw 0x0000, 0x00C6, 0x0000, 0x00AA, 0x0000, 0x0000, 0x0000, 0x0000
        dw 0x0141, 0x00D8, 0x0152, 0x00BA, 0x0000, 0x0000, 0x0000, 0x0000
        dw 0x0000, 0x00E6, 0x0000, 0x0000, 0x0000, 0x0131, 0x0000, 0x0000
        dw 0x0142, 0x00F8, 0x0153, 0x00DF, 0x0000, 0x0000, 0x0000, 0x0000
sin_acento:                             ; de una letra 437 (80-FF), la ASCII que se le parece
        db 0x43, 0x75, 0x65, 0x61, 0x61, 0x61, 0x61, 0x63, 0x65, 0x65, 0x65, 0x69
        db 0x69, 0x69, 0x41, 0x41, 0x45, 0x6E, 0x6E, 0x6F, 0x6F, 0x6F, 0x75, 0x75
        db 0x79, 0x4F, 0x55, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x61, 0x69, 0x6F, 0x75
        db 0x6E, 0x4E, 0x61, 0x6F, 0x6E, 0x6E, 0x6E, 0x31, 0x31, 0x6E, 0x6E, 0x6E
        db 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E
        db 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E
        db 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E
        db 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E
        db 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E
        db 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E
        db 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x32, 0x6E, 0x6E
an_std:                                 ; anchos de 20 a 7E: Helvetica, Helvetica-Bold, Times-Roman, Times-Bold
        dw 278, 278, 355, 556, 556, 889, 667, 191, 333, 333, 389, 584
        dw 278, 333, 278, 278, 556, 556, 556, 556, 556, 556, 556, 556
        dw 556, 556, 278, 278, 584, 584, 584, 556, 1015, 667, 667, 722
        dw 722, 667, 611, 778, 722, 278, 500, 667, 556, 833, 722, 778
        dw 667, 778, 722, 667, 611, 722, 667, 944, 667, 667, 611, 278
        dw 278, 278, 469, 556, 333, 556, 556, 500, 556, 556, 278, 556
        dw 556, 222, 222, 500, 222, 833, 556, 556, 556, 556, 333, 500
        dw 278, 556, 500, 722, 500, 500, 500, 334, 260, 334, 584
        dw 278, 333, 474, 556, 556, 889, 722, 238, 333, 333, 389, 584
        dw 278, 333, 278, 278, 556, 556, 556, 556, 556, 556, 556, 556
        dw 556, 556, 333, 333, 584, 584, 584, 611, 975, 722, 722, 722
        dw 722, 667, 611, 778, 722, 278, 556, 722, 611, 833, 722, 778
        dw 667, 778, 722, 667, 611, 722, 667, 944, 667, 667, 611, 333
        dw 278, 333, 584, 556, 333, 556, 611, 556, 611, 556, 333, 611
        dw 611, 278, 278, 556, 278, 889, 611, 611, 611, 611, 389, 556
        dw 333, 611, 556, 778, 556, 556, 500, 389, 280, 389, 584
        dw 250, 333, 408, 500, 500, 833, 778, 180, 333, 333, 500, 564
        dw 250, 333, 250, 278, 500, 500, 500, 500, 500, 500, 500, 500
        dw 500, 500, 278, 278, 564, 564, 564, 444, 921, 722, 667, 667
        dw 722, 611, 556, 722, 722, 333, 389, 722, 611, 889, 722, 722
        dw 556, 722, 667, 556, 611, 722, 722, 944, 722, 722, 611, 333
        dw 278, 333, 469, 500, 333, 444, 500, 444, 500, 444, 333, 500
        dw 500, 278, 278, 500, 278, 778, 500, 500, 500, 500, 333, 389
        dw 278, 500, 500, 722, 500, 500, 444, 480, 200, 480, 541
        dw 250, 333, 555, 500, 500, 1000, 833, 278, 333, 333, 500, 570
        dw 250, 333, 250, 278, 500, 500, 500, 500, 500, 500, 500, 500
        dw 500, 500, 333, 333, 570, 570, 570, 500, 930, 722, 667, 722
        dw 722, 667, 611, 778, 778, 389, 500, 778, 667, 944, 722, 778
        dw 611, 778, 722, 556, 667, 722, 722, 1000, 722, 722, 667, 333
        dw 278, 333, 581, 500, 333, 500, 556, 444, 556, 444, 333, 500
        dw 556, 278, 333, 556, 278, 833, 556, 500, 556, 556, 444, 389
        dw 333, 556, 500, 722, 500, 500, 444, 394, 220, 394, 520
exp_tab:                                ; de cada código de 0 a 15, sus letras (4 octetos, acabadas en 0)
        db 0, 0, 0, 0
        db "fi", 0, 0
        db "fl", 0, 0
        db "ff", 0, 0
        db "ffi", 0
        db "ffl", 0
        db "EUR", 0
        db "...", 0
        db "OE", 0, 0
        db 0, 0, 0, 0
        db 0, 0, 0, 0
        db "oe", 0, 0
        db "TM", 0, 0
        db 0, 0, 0, 0
        db "(c)", 0
        db "(R)", 0
gl_tab:                                 ; db largo, nombre, dw Unicode; acaba en 0
        db 2, "AE"
        dw 0x00C6
        db 6, "Aacute"
        dw 0x00C1
        db 11, "Acircumflex"
        dw 0x00C2
        db 9, "Adieresis"
        dw 0x00C4
        db 6, "Agrave"
        dw 0x00C0
        db 7, "Aogonek"
        dw 0x0104
        db 5, "Aring"
        dw 0x00C5
        db 6, "Atilde"
        dw 0x00C3
        db 6, "Cacute"
        dw 0x0106
        db 6, "Ccaron"
        dw 0x010C
        db 8, "Ccedilla"
        dw 0x00C7
        db 6, "Eacute"
        dw 0x00C9
        db 6, "Ecaron"
        dw 0x011A
        db 11, "Ecircumflex"
        dw 0x00CA
        db 9, "Edieresis"
        dw 0x00CB
        db 6, "Egrave"
        dw 0x00C8
        db 7, "Eogonek"
        dw 0x0118
        db 3, "Eth"
        dw 0x00D0
        db 4, "Euro"
        dw 0x20AC
        db 5, "Gamma"
        dw 0x0393
        db 6, "Gbreve"
        dw 0x011E
        db 6, "Iacute"
        dw 0x00CD
        db 11, "Icircumflex"
        dw 0x00CE
        db 9, "Idieresis"
        dw 0x00CF
        db 10, "Idotaccent"
        dw 0x0130
        db 6, "Igrave"
        dw 0x00CC
        db 6, "Lslash"
        dw 0x0141
        db 6, "Nacute"
        dw 0x0143
        db 6, "Ntilde"
        dw 0x00D1
        db 2, "OE"
        dw 0x0152
        db 6, "Oacute"
        dw 0x00D3
        db 11, "Ocircumflex"
        dw 0x00D4
        db 9, "Odieresis"
        dw 0x00D6
        db 6, "Ograve"
        dw 0x00D2
        db 5, "Omega"
        dw 0x03A9
        db 6, "Oslash"
        dw 0x00D8
        db 6, "Otilde"
        dw 0x00D5
        db 3, "Phi"
        dw 0x03A6
        db 6, "Rcaron"
        dw 0x0158
        db 6, "Sacute"
        dw 0x015A
        db 6, "Scaron"
        dw 0x0160
        db 8, "Scedilla"
        dw 0x015E
        db 5, "Sigma"
        dw 0x03A3
        db 5, "Theta"
        dw 0x0398
        db 5, "Thorn"
        dw 0x00DE
        db 6, "Uacute"
        dw 0x00DA
        db 11, "Ucircumflex"
        dw 0x00DB
        db 9, "Udieresis"
        dw 0x00DC
        db 6, "Ugrave"
        dw 0x00D9
        db 5, "Uring"
        dw 0x016E
        db 6, "Yacute"
        dw 0x00DD
        db 9, "Ydieresis"
        dw 0x0178
        db 6, "Zacute"
        dw 0x0179
        db 6, "Zcaron"
        dw 0x017D
        db 10, "Zdotaccent"
        dw 0x017B
        db 6, "aacute"
        dw 0x00E1
        db 11, "acircumflex"
        dw 0x00E2
        db 5, "acute"
        dw 0x00B4
        db 9, "adieresis"
        dw 0x00E4
        db 2, "ae"
        dw 0x00E6
        db 6, "agrave"
        dw 0x00E0
        db 5, "alpha"
        dw 0x03B1
        db 9, "ampersand"
        dw 0x0026
        db 7, "aogonek"
        dw 0x0105
        db 11, "approxequal"
        dw 0x2248
        db 5, "aring"
        dw 0x00E5
        db 9, "arrowleft"
        dw 0x2190
        db 10, "arrowright"
        dw 0x2192
        db 11, "asciicircum"
        dw 0x005E
        db 10, "asciitilde"
        dw 0x007E
        db 8, "asterisk"
        dw 0x002A
        db 12, "asteriskmath"
        dw 0x2217
        db 2, "at"
        dw 0x0040
        db 6, "atilde"
        dw 0x00E3
        db 9, "backslash"
        dw 0x005C
        db 3, "bar"
        dw 0x007C
        db 4, "beta"
        dw 0x03B2
        db 9, "braceleft"
        dw 0x007B
        db 10, "braceright"
        dw 0x007D
        db 11, "bracketleft"
        dw 0x005B
        db 12, "bracketright"
        dw 0x005D
        db 5, "breve"
        dw 0x02D8
        db 9, "brokenbar"
        dw 0x00A6
        db 6, "bullet"
        dw 0x2022
        db 6, "cacute"
        dw 0x0107
        db 5, "caron"
        dw 0x02C7
        db 6, "ccaron"
        dw 0x010D
        db 8, "ccedilla"
        dw 0x00E7
        db 7, "cedilla"
        dw 0x00B8
        db 4, "cent"
        dw 0x00A2
        db 10, "circumflex"
        dw 0x02C6
        db 5, "colon"
        dw 0x003A
        db 5, "comma"
        dw 0x002C
        db 9, "copyright"
        dw 0x00A9
        db 8, "currency"
        dw 0x00A4
        db 6, "dagger"
        dw 0x2020
        db 9, "daggerdbl"
        dw 0x2021
        db 6, "degree"
        dw 0x00B0
        db 5, "delta"
        dw 0x03B4
        db 8, "dieresis"
        dw 0x00A8
        db 6, "divide"
        dw 0x00F7
        db 6, "dollar"
        dw 0x0024
        db 9, "dotaccent"
        dw 0x02D9
        db 8, "dotlessi"
        dw 0x0131
        db 6, "eacute"
        dw 0x00E9
        db 6, "ecaron"
        dw 0x011B
        db 11, "ecircumflex"
        dw 0x00EA
        db 9, "edieresis"
        dw 0x00EB
        db 6, "egrave"
        dw 0x00E8
        db 5, "eight"
        dw 0x0038
        db 8, "ellipsis"
        dw 0x2026
        db 6, "emdash"
        dw 0x2014
        db 6, "endash"
        dw 0x2013
        db 7, "eogonek"
        dw 0x0119
        db 7, "epsilon"
        dw 0x03B5
        db 5, "equal"
        dw 0x003D
        db 3, "eth"
        dw 0x00F0
        db 4, "euro"
        dw 0x20AC
        db 6, "exclam"
        dw 0x0021
        db 10, "exclamdown"
        dw 0x00A1
        db 2, "ff"
        dw 0xFB00
        db 3, "ffi"
        dw 0xFB03
        db 3, "ffl"
        dw 0xFB04
        db 2, "fi"
        dw 0xFB01
        db 4, "five"
        dw 0x0035
        db 2, "fl"
        dw 0xFB02
        db 6, "florin"
        dw 0x0192
        db 4, "four"
        dw 0x0034
        db 8, "fraction"
        dw 0x2044
        db 6, "gbreve"
        dw 0x011F
        db 10, "germandbls"
        dw 0x00DF
        db 5, "grave"
        dw 0x0060
        db 7, "greater"
        dw 0x003E
        db 12, "greaterequal"
        dw 0x2265
        db 13, "guillemotleft"
        dw 0x00AB
        db 14, "guillemotright"
        dw 0x00BB
        db 13, "guilsinglleft"
        dw 0x2039
        db 14, "guilsinglright"
        dw 0x203A
        db 12, "hungarumlaut"
        dw 0x02DD
        db 6, "hyphen"
        dw 0x002D
        db 11, "hyphenminus"
        dw 0x002D
        db 6, "iacute"
        dw 0x00ED
        db 11, "icircumflex"
        dw 0x00EE
        db 9, "idieresis"
        dw 0x00EF
        db 6, "igrave"
        dw 0x00EC
        db 8, "infinity"
        dw 0x221E
        db 4, "less"
        dw 0x003C
        db 9, "lessequal"
        dw 0x2264
        db 10, "logicalnot"
        dw 0x00AC
        db 6, "lslash"
        dw 0x0142
        db 6, "macron"
        dw 0x00AF
        db 6, "middot"
        dw 0x00B7
        db 5, "minus"
        dw 0x2212
        db 2, "mu"
        dw 0x00B5
        db 8, "multiply"
        dw 0x00D7
        db 6, "nacute"
        dw 0x0144
        db 7, "nbspace"
        dw 0x00A0
        db 4, "nine"
        dw 0x0039
        db 16, "nonbreakingspace"
        dw 0x00A0
        db 6, "ntilde"
        dw 0x00F1
        db 10, "numbersign"
        dw 0x0023
        db 6, "oacute"
        dw 0x00F3
        db 11, "ocircumflex"
        dw 0x00F4
        db 9, "odieresis"
        dw 0x00F6
        db 2, "oe"
        dw 0x0153
        db 6, "ogonek"
        dw 0x02DB
        db 6, "ograve"
        dw 0x00F2
        db 3, "one"
        dw 0x0031
        db 7, "onehalf"
        dw 0x00BD
        db 10, "onequarter"
        dw 0x00BC
        db 11, "onesuperior"
        dw 0x00B9
        db 11, "ordfeminine"
        dw 0x00AA
        db 12, "ordmasculine"
        dw 0x00BA
        db 6, "oslash"
        dw 0x00F8
        db 6, "otilde"
        dw 0x00F5
        db 9, "paragraph"
        dw 0x00B6
        db 9, "parenleft"
        dw 0x0028
        db 10, "parenright"
        dw 0x0029
        db 7, "percent"
        dw 0x0025
        db 6, "period"
        dw 0x002E
        db 14, "periodcentered"
        dw 0x00B7
        db 14, "periodinferior"
        dw 0x002E
        db 11, "perthousand"
        dw 0x2030
        db 3, "phi"
        dw 0x03C6
        db 2, "pi"
        dw 0x03C0
        db 4, "plus"
        dw 0x002B
        db 9, "plusminus"
        dw 0x00B1
        db 8, "question"
        dw 0x003F
        db 12, "questiondown"
        dw 0x00BF
        db 8, "quotedbl"
        dw 0x0022
        db 12, "quotedblbase"
        dw 0x201E
        db 12, "quotedblleft"
        dw 0x201C
        db 13, "quotedblright"
        dw 0x201D
        db 9, "quoteleft"
        dw 0x2018
        db 13, "quotereversed"
        dw 0x201B
        db 10, "quoteright"
        dw 0x2019
        db 14, "quotesinglbase"
        dw 0x201A
        db 11, "quotesingle"
        dw 0x0027
        db 7, "radical"
        dw 0x221A
        db 6, "rcaron"
        dw 0x0159
        db 10, "registered"
        dw 0x00AE
        db 4, "ring"
        dw 0x02DA
        db 6, "sacute"
        dw 0x015B
        db 6, "scaron"
        dw 0x0161
        db 8, "scedilla"
        dw 0x015F
        db 7, "section"
        dw 0x00A7
        db 9, "semicolon"
        dw 0x003B
        db 5, "seven"
        dw 0x0037
        db 9, "sfthyphen"
        dw 0x00AD
        db 5, "sigma"
        dw 0x03C3
        db 3, "six"
        dw 0x0036
        db 5, "slash"
        dw 0x002F
        db 10, "softhyphen"
        dw 0x00AD
        db 5, "space"
        dw 0x0020
        db 8, "sterling"
        dw 0x00A3
        db 3, "tau"
        dw 0x03C4
        db 5, "thorn"
        dw 0x00FE
        db 5, "three"
        dw 0x0033
        db 13, "threequarters"
        dw 0x00BE
        db 13, "threesuperior"
        dw 0x00B3
        db 5, "tilde"
        dw 0x02DC
        db 9, "trademark"
        dw 0x2122
        db 3, "two"
        dw 0x0032
        db 11, "twosuperior"
        dw 0x00B2
        db 6, "uacute"
        dw 0x00FA
        db 11, "ucircumflex"
        dw 0x00FB
        db 9, "udieresis"
        dw 0x00FC
        db 6, "ugrave"
        dw 0x00F9
        db 10, "underscore"
        dw 0x005F
        db 13, "underscoredbl"
        dw 0x005F
        db 5, "uring"
        dw 0x016F
        db 6, "yacute"
        dw 0x00FD
        db 9, "ydieresis"
        dw 0x00FF
        db 3, "yen"
        dw 0x00A5
        db 6, "zacute"
        dw 0x017A
        db 6, "zcaron"
        dw 0x017E
        db 10, "zdotaccent"
        dw 0x017C
        db 4, "zero"
        dw 0x0030
        db 0
