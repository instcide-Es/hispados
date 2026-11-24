/* Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * nucleo64/programas/ventanas/hdisp.h - Hispa-64, etapa 8: lo de los
 * dispositivos del núcleo para los programas (disp64.cas;
 * docs/64/HISPA-64.md, sección 14), con los nombres y la forma de Linux
 * (musl no trae las cabeceras del núcleo de Linux): /dev/fb0 (fbdev) y
 * /dev/input/event0 (evdev), y lo propio de Hispa-64 (FBIO_HISPA_COPIA y
 * EV_LETRA). */
#ifndef HDISP_H
#define HDISP_H
#include <stdint.h>
#include <sys/time.h>

/* ---- /dev/fb0 ---- */
#define FBIOGET_VSCREENINFO 0x4600
#define FBIOPUT_VSCREENINFO 0x4601
#define FBIOGET_FSCREENINFO 0x4602
#define FBIOPAN_DISPLAY 0x4606
#define FB_VISUAL_TRUECOLOR 2
/* (de Hispa-64) copia un rectángulo del búfer de atrás (la mitad de abajo
 * de lo que da mmap: desde smem_len / 2) a la pantalla: uint32_t[4] =
 * x, y, ancho, alto */
#define FBIO_HISPA_COPIA 0x46E0

struct fb_bitfield {
	uint32_t offset, length, msb_right;
};
struct fb_var_screeninfo {
	uint32_t xres, yres, xres_virtual, yres_virtual, xoffset, yoffset;
	uint32_t bits_per_pixel, grayscale;
	struct fb_bitfield red, green, blue, transp;
	uint32_t nonstd, activate, height, width, accel_flags;
	uint32_t pixclock, left_margin, right_margin, upper_margin, lower_margin;
	uint32_t hsync_len, vsync_len, sync, vmode, rotate, colorspace;
	uint32_t reserved[4];
};
struct fb_fix_screeninfo {
	char id[16];
	unsigned long smem_start;
	uint32_t smem_len, type, type_aux, visual;
	uint16_t xpanstep, ypanstep, ywrapstep;
	uint32_t line_length;
	unsigned long mmio_start;
	uint32_t mmio_len, accel;
	uint16_t capabilities, reserved[2];
};

/* ---- /dev/input/event0 ---- */
struct input_event {
	struct timeval time;
	uint16_t type, code;
	int32_t value;
};
#define EV_SYN 0
#define EV_KEY 1
#define EV_REL 2
#define EV_ABS 3
/* (de Hispa-64) la letra ya hecha con el mapa del teclado: value = Unicode,
 * o el control (13 Intro, 127 Retroceso, 9 Tab, 27 Esc, Ctrl+A..Z = 1..26) */
#define EV_LETRA 0x1E
#define ABS_X 0
#define ABS_Y 1
#define REL_WHEEL 8
#define BTN_LEFT 0x110
#define BTN_RIGHT 0x111
#define BTN_MIDDLE 0x112
#define EVIOCGVERSION 0x80044501u
#define EVIOCGID 0x80084502u
#define EVIOCGNAME(n) (0x80004506u | ((unsigned)(n) << 16))
#define EVIOCGRAB 0x40044590u

/* las teclas de Linux que hacen falta */
#define KEY_ESC 1
#define KEY_BACKSPACE 14
#define KEY_TAB 15
#define KEY_ENTER 28
#define KEY_LEFTCTRL 29
#define KEY_LEFTSHIFT 42
#define KEY_RIGHTSHIFT 54
#define KEY_LEFTALT 56
#define KEY_F1 59
#define KEY_F10 68
#define KEY_F11 87
#define KEY_F12 88
#define KEY_KPENTER 96
#define KEY_RIGHTCTRL 97
#define KEY_RIGHTALT 100
#define KEY_HOME 102
#define KEY_UP 103
#define KEY_PAGEUP 104
#define KEY_LEFT 105
#define KEY_RIGHT 106
#define KEY_END 107
#define KEY_DOWN 108
#define KEY_PAGEDOWN 109
#define KEY_INSERT 110
#define KEY_DELETE 111
#define KEY_LEFTMETA 125
#define KEY_RIGHTMETA 126
#endif
