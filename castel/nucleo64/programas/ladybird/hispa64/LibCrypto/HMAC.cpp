/*
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * nucleo64/programas/ladybird/hispa64/LibCrypto/HMAC.cpp - Hispa-64, etapa 10 f:
 * LibCrypto/Authentication/HMAC.h de Ladybird (la misma interfaz), sin
 * OpenSSL: HMAC (RFC 2104) sobre los resúmenes de Hash.cpp, con el tamaño de
 * bloque de cada uno (SHA-1 y SHA-256, 64; SHA-384 y SHA-512, 128; SHA-3, su
 * tasa: 136, 104 y 72, como en FIPS 202 y la NIST).
 */

#include <AK/Array.h>
#include <LibCrypto/Authentication/HMAC.h>

namespace Crypto::Authentication {

static size_t tamaño_de_bloque(Hash::HashKind kind)
{
    switch (kind) {
    case Hash::HashKind::SHA1:
    case Hash::HashKind::SHA256:
        return 64;
    case Hash::HashKind::SHA384:
    case Hash::HashKind::SHA512:
        return 128;
    case Hash::HashKind::SHA3_256:
        return 136;
    case Hash::HashKind::SHA3_384:
        return 104;
    case Hash::HashKind::SHA3_512:
        return 72;
    default:
        return 0;
    }
}

ErrorOr<Hash::Digest> hmac(Hash::HashKind hash_kind, ReadonlyBytes key, ReadonlyBytes message)
{
    TRY(Hash::openssl_digest_name(hash_kind));
    auto bloque = tamaño_de_bloque(hash_kind);
    Array<u8, 136> k {};
    if (key.size() > bloque) {
        auto d = Hash::digest(hash_kind, key);
        __builtin_memcpy(k.data(), d.data.data(), d.size);
    } else if (!key.is_empty()) {
        __builtin_memcpy(k.data(), key.data(), key.size());
    }
    Array<u8, 136> ipad {}, opad {};
    for (size_t i = 0; i < bloque; i++) {
        ipad[i] = k[i] ^ 0x36;
        opad[i] = k[i] ^ 0x5c;
    }
    Hash::Hasher dentro(hash_kind);
    dentro.update(ReadonlyBytes { ipad.data(), bloque });
    dentro.update(message);
    auto d = dentro.finish();
    Hash::Hasher fuera(hash_kind);
    fuera.update(ReadonlyBytes { opad.data(), bloque });
    fuera.update(d.bytes());
    return fuera.finish();
}

}
