/* tests/gl/terceros/glad/glad_wgl.h - lo de WGL que mira wglgears: sin
 * WGL_ARB_pixel_format (HispaGL no lo da: ni sRGB ni multimuestreo) y con
 * WGL_EXT_swap_control (wglGetSwapIntervalEXT, de OPENGL32.DLL).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "../windows.h"
#define GLAD_WGL_ARB_create_context 0
#define GLAD_WGL_ARB_pixel_format 0
#define GLAD_WGL_EXT_swap_control 1
#define GL_FRAMEBUFFER_SRGB 0x8DB9            /* (no se pide: sin -srgb) */
#define WGL_DRAW_TO_WINDOW_ARB 0x2001
#define WGL_SUPPORT_OPENGL_ARB 0x2010
#define WGL_DOUBLE_BUFFER_ARB 0x2011
#define WGL_COLOR_BITS_ARB 0x2014
#define WGL_DEPTH_BITS_ARB 0x2022
#define WGL_SAMPLE_BUFFERS_ARB 0x2041
#define WGL_SAMPLES_ARB 0x2042
#define WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB 0x20A9
int WINAPI wglGetSwapIntervalEXT(void);
static int gladLoadWGL(HDC h) { (void)h; return 1; }
static BOOL wglChoosePixelFormatARB(HDC h, const int *a, const float *b, UINT n, int *f, UINT *m) { (void)h; (void)a; (void)b; (void)n; (void)f; (void)m; return FALSE; }
