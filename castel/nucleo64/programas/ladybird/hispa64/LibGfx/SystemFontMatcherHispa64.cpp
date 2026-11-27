/*
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * hispa64/LibGfx/SystemFontMatcherHispa64.cpp - Hispa-64, etapa 10 g
 * (docs/64/HISPA-64.md, sección 24): el buscador de letras del sistema de
 * LibGfx para Hispa-64, en vez de SystemFontMatcherNone.cpp (el de Ladybird
 * sin fontconfig, que no sabe ninguna). Ladybird carga todas las letras de
 * las carpetas de datos (XDG_DATA_DIRS/fonts), pero las familias genéricas
 * (sans-serif, serif, monospace) las busca en una lista de nombres (Arial,
 * Helvetica, Times...) en la que no está DejaVu Sans, y sin ninguna de ellas
 * WebContent se para al empezar (StyleComputer: la letra por omisión, nula).
 * Aquí, cada familia genérica es una DejaVu (las que lleva Hispa-64,
 * terceros/dejavu): sans-serif y las demás de letra de palo, «DejaVu Sans»;
 * serif y math, «DejaVu Serif»; monospace, «DejaVu Sans Mono». Si no está,
 * FontPlugin sigue con su lista. Lo demás, como el de Ladybird sin
 * fontconfig: sin buscar por familia ni por carácter fuera de lo cargado.
 */
#include <AK/StringView.h>
#include <LibGfx/Font/SystemFontMatcher.h>

namespace Gfx::SystemFontMatcher {

Optional<SystemFontMatch> match_family_style(StringView, u16, u16, u8)
{
    return {};
}

Optional<SystemFontMatch> match_code_point(u32, u16, u16, u8, bool)
{
    return {};
}

Optional<FlyString> resolve_generic_family(StringView family, u16, u8)
{
    if (family.equals_ignoring_ascii_case("monospace"sv) || family.equals_ignoring_ascii_case("ui-monospace"sv))
        return "DejaVu Sans Mono"_fly_string;
    if (family.equals_ignoring_ascii_case("serif"sv) || family.equals_ignoring_ascii_case("ui-serif"sv) || family.equals_ignoring_ascii_case("math"sv))
        return "DejaVu Serif"_fly_string;
    return "DejaVu Sans"_fly_string;
}

}
