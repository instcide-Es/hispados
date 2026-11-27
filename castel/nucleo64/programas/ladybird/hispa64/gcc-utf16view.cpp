/* Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * nucleo64/programas/ladybird/hispa64/gcc-utf16view.cpp - Hispa-64, etapa 10 f
 * (docs/64/HISPA-64.md, sección 23): la capa de Hispa-64 para Ladybird. Un
 * arreglo para GCC: Utf16View tiene un constructor «consteval» desde un
 * StringView (texto ASCII conocido al compilar) y, en Tests/AK/TestUtf16String.cpp,
 * se llega a él por std::construct_at (al hacer un Optional<Utf16View> desde
 * un literal); clang lo evalúa al compilar, pero GCC 14 deja la llamada a
 * construct_at sin emitir (una función consteval no se puede llamar en
 * marcha) y el enlace falla. Aquí va esa función: lo mismo que hace el
 * constructor (una vista Latin-1 de solo ASCII), con from_ascii, que es
 * constexpr y da la misma vista. */
#include <AK/Utf16View.h>
#include <memory>

namespace std {

template<>
AK::Utf16View* construct_at<AK::Utf16View, AK::StringView>(AK::Utf16View* sitio, AK::StringView&& texto)
{
    return ::new (static_cast<void*>(sitio)) AK::Utf16View(AK::Utf16View::from_ascii(texto));
}

}
