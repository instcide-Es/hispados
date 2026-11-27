/*
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * nucleo64/programas/ladybird/hispa64/LibCrypto/EC.cpp - Hispa-64, etapa 10 f:
 * LibCrypto/PK/EC.h de Ladybird (la misma interfaz), con BearSSL en vez de
 * OpenSSL: comprobar una firma ECDSA (r y s, en bruto) de un resumen, con la
 * clave pública por sus coordenadas, en P-256, P-384 o P-521 (por el tamaño
 * de x: 32, 48 o 66). Como en la de Ladybird, una clave que no es un punto
 * de la curva es un error, y unas r o s de otro tamaño, «no vale».
 */

#include <AK/ByteBuffer.h>
#include <LibCrypto/PK/EC.h>

#include <bearssl.h>

namespace Crypto::PK {

static ErrorOr<int> curva(size_t tam)
{
    switch (tam) {
    case 32:
        return BR_EC_secp256r1;
    case 48:
        return BR_EC_secp384r1;
    case 66:
        return BR_EC_secp521r1;
    default:
        return Error::from_string_literal("Unsupported EC curve");
    }
}

ErrorOr<bool> ecdsa_verify(ReadonlyBytes x, ReadonlyBytes y, ReadonlyBytes digest, ReadonlyBytes r, ReadonlyBytes s)
{
    if (x.size() != y.size())
        return Error::from_string_literal("EC coordinates have different sizes");
    auto id = TRY(curva(x.size()));
    auto punto = TRY(ByteBuffer::create_uninitialized(1 + x.size() + y.size()));
    punto[0] = 0x04;
    punto.overwrite(1, x.data(), x.size());
    punto.overwrite(1 + x.size(), y.data(), y.size());
    auto const* ec = &br_ec_all_m31;
    // ¿es un punto de la curva? (mul lo decodifica y lo comprueba; por 1, queda igual)
    auto prueba = TRY(ByteBuffer::copy(punto));
    static unsigned char const uno[1] = { 1 };
    if (!ec->mul(prueba.data(), prueba.size(), uno, 1, id))
        return Error::from_string_literal("Invalid EC public key");
    if (r.size() != x.size() || s.size() != x.size())
        return false;
    auto firma = TRY(ByteBuffer::create_uninitialized(r.size() + s.size()));
    firma.overwrite(0, r.data(), r.size());
    firma.overwrite(r.size(), s.data(), s.size());
    br_ec_public_key clave { id, punto.data(), punto.size() };
    return br_ecdsa_i31_vrfy_raw(ec, digest.data(), digest.size(), &clave, firma.data(), firma.size()) == 1;
}

}
