/* tests/gl/terceros/glad/glad.h - en vez del cargador «glad»: OPENGL32.DLL
 * de HispaGL ya exporta todo OpenGL 1.1 (la importación es directa).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "../windows.h"
static int gladLoadGL(void) { return 1; }
