/*
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * hispa64/LibMediaCodecs/FFmpegFunctions.cpp - Hispa-64, etapa 10 g
 * (docs/64/HISPA-64.md, sección 24): FFmpegFunctions.cpp de LibMediaCodecs de
 * Ladybird, sin FFmpeg. En Linux, Ladybird carga su FFmpeg al correr
 * (dlopen de libavcodec y libswresample), y WebContent no arranca sin él
 * (main.cpp: TRY(load_bundled_ffmpeg())). Hispa-64 no lleva FFmpeg y sus
 * programas son estáticos (dlopen no puede): aquí la tabla de funciones de
 * FFmpeg se llena con funciones que no hacen nada y devuelven el valor por
 * omisión de su tipo (nulo, 0), con su tipo exacto (las de FFmpegFunctions.h);
 * las versiones, las de las cabeceras (terceros/ffmpeg-cabeceras). Así
 * WebContent arranca, y el vídeo y el sonido de FFmpeg no van: no hay
 * descodificador (avcodec_find_decoder da nulo) y Ladybird lo dice como con
 * un formato que no conoce.
 * Se pone en vez del de Ladybird en la raíz (hispa64/cmake/raiz), sin
 * cambiarlo.
 */
#include <AK/Error.h>
#include <LibMediaCodecs/FFmpeg/BundledFFmpeg.h>
#include <LibMediaCodecs/FFmpeg/FFmpegFunctions.h>
#include <LibMediaCodecs/FFmpeg/FFmpegHelpers.h>
#include <errno.h>
#include <new>
#include <type_traits>

namespace FFMPEG_ABI_NAMESPACE {

// una función que no hace nada, del tipo de cada puntero de la tabla
template<typename>
struct Nada;
template<typename R, typename... A>
struct Nada<R (*)(A...)> {
    static R f(A...)
    {
        if constexpr (!std::is_void_v<R>)
            return R {};
    }
};

static unsigned version_avcodec() { return LIBAVCODEC_VERSION_INT; }
static unsigned version_avutil() { return LIBAVUTIL_VERSION_INT; }

static FFmpegFunctions const* s_bundled_functions = nullptr;

ErrorOr<void> load_bundled_ffmpeg()
{
    if (s_bundled_functions != nullptr)
        return {};
    auto* functions = new (std::nothrow) FFmpegFunctions;
    if (functions == nullptr)
        return Error::from_errno(ENOMEM);
#define NADA(name) functions->name = &Nada<FFmpegFunctionPointers::name>::f;
    FFMPEG_ENUMERATE_FUNCTIONS(NADA)
    FFMPEG_ENUMERATE_BUNDLED_FUNCTIONS(NADA)
#undef NADA
    functions->avcodec_version = version_avcodec;
    functions->avutil_version = version_avutil;
    functions->codec_ids = codec_ids_of_these_headers();
    s_bundled_functions = functions;
    return {};
}

FFmpegFunctions const& FFmpegFunctions::bundled()
{
    VERIFY(s_bundled_functions != nullptr);
    return *s_bundled_functions;
}

}
