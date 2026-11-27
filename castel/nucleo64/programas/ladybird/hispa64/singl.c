/*
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * hispa64/singl.c - Hispa-64, etapa 10 g (docs/64/HISPA-64.md, sección 24):
 * las funciones de EGL y OpenGL ES que nombra el Compositor de Ladybird
 * (OpenGLContext.cpp y SkiaBackendContext, para WebGL y el dibujo por la
 * tarjeta), sin ANGLE: no hay pantalla de EGL (eglGetPlatformDisplay da
 * EGL_NO_DISPLAY, eglInitialize EGL_FALSE, eglGetProcAddress nulo), así que
 * no hay WebGL y Ladybird dibuja con Skia por el procesador. Las de GLES no
 * se llaman sin contexto: no hacen nada. Todas devuelven 0 en rax (nulo,
 * EGL_FALSE, EGL_NO_DISPLAY, EGL_NO_CONTEXT para sus tipos); van sin su
 * prototipo (las cabeceras, terceros/angle-cabeceras, no se incluyen aquí).
 */
#define NO_HAY(f) long f(void); long f(void) { return 0; }
NO_HAY(eglChooseConfig)
NO_HAY(eglCreateContext)
NO_HAY(eglDestroyContext)
NO_HAY(eglDestroySurface)
NO_HAY(eglGetCurrentContext)
NO_HAY(eglGetPlatformDisplay)
NO_HAY(eglGetProcAddress)
NO_HAY(eglInitialize)
NO_HAY(eglMakeCurrent)
NO_HAY(glBindBuffer)
NO_HAY(glBindFramebuffer)
NO_HAY(glBindRenderbuffer)
NO_HAY(glBindTexture)
NO_HAY(glCheckFramebufferStatus)
NO_HAY(glCopySubTextureCHROMIUM)
NO_HAY(glDeleteFramebuffers)
NO_HAY(glDeleteRenderbuffers)
NO_HAY(glDeleteTextures)
NO_HAY(glFinish)
NO_HAY(glFramebufferRenderbuffer)
NO_HAY(glFramebufferTexture2D)
NO_HAY(glGenFramebuffers)
NO_HAY(glGenRenderbuffers)
NO_HAY(glGenTextures)
NO_HAY(glGetIntegerv)
NO_HAY(glGetString)
NO_HAY(glPixelStorei)
NO_HAY(glReadBuffer)
NO_HAY(glReadPixelsRobustANGLE)
NO_HAY(glRenderbufferStorage)
NO_HAY(glTexImage2D)
NO_HAY(glTexParameteri)
NO_HAY(glViewport)
