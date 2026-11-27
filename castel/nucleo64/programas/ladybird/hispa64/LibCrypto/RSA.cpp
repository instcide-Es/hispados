/*
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * nucleo64/programas/ladybird/hispa64/LibCrypto/RSA.cpp - Hispa-64, etapa 10 f:
 * LibCrypto/PK/RSA.h de Ladybird (la misma interfaz), con BearSSL en vez de
 * OpenSSL: comprobar una firma RSA PKCS #1 v1.5 (RFC 8017, 8.2.2) con las
 * mismas condiciones que la de Ladybird: e impar, mayor que 1 y menor que n;
 * n impar; la firma, del tamaño de n.
 */

#include <LibCrypto/PK/RSA.h>

#include <bearssl.h>

namespace Crypto::PK {

// (sin los ceros de delante, como BN_bin2bn)
static ReadonlyBytes sin_ceros(ReadonlyBytes b)
{
    size_t i = 0;
    while (i < b.size() && b[i] == 0)
        i++;
    return b.slice(i);
}

// e < n, los dos sin ceros delante
static bool menor(ReadonlyBytes e, ReadonlyBytes n)
{
    if (e.size() != n.size())
        return e.size() < n.size();
    for (size_t i = 0; i < e.size(); i++)
        if (e[i] != n[i])
            return e[i] < n[i];
    return false;
}

// el OID de cada resumen (DigestInfo), con su largo delante, como los BR_HASH_OID_* de BearSSL
static unsigned char const* oid_de(Hash::HashKind kind)
{
    switch (kind) {
    case Hash::HashKind::SHA1:
        return reinterpret_cast<unsigned char const*>(BR_HASH_OID_SHA1);
    case Hash::HashKind::SHA256:
        return reinterpret_cast<unsigned char const*>(BR_HASH_OID_SHA256);
    case Hash::HashKind::SHA384:
        return reinterpret_cast<unsigned char const*>(BR_HASH_OID_SHA384);
    case Hash::HashKind::SHA512:
        return reinterpret_cast<unsigned char const*>(BR_HASH_OID_SHA512);
    case Hash::HashKind::SHA3_256:
        return reinterpret_cast<unsigned char const*>("\x09\x60\x86\x48\x01\x65\x03\x04\x02\x08");
    case Hash::HashKind::SHA3_384:
        return reinterpret_cast<unsigned char const*>("\x09\x60\x86\x48\x01\x65\x03\x04\x02\x09");
    case Hash::HashKind::SHA3_512:
        return reinterpret_cast<unsigned char const*>("\x09\x60\x86\x48\x01\x65\x03\x04\x02\x0a");
    default:
        return nullptr;
    }
}

ErrorOr<bool> rsa_pkcs1_verify(ReadonlyBytes modulus, ReadonlyBytes public_exponent, Hash::HashKind hash_kind, ReadonlyBytes message, ReadonlyBytes signature)
{
    TRY(Hash::openssl_digest_name(hash_kind));
    auto n = sin_ceros(modulus);
    auto e = sin_ceros(public_exponent);
    if (n.is_empty() || e.is_empty())
        return Error::from_string_literal("Invalid RSA public key");
    // https://www.rfc-editor.org/rfc/rfc8017#section-3.1: e entre 3 y n - 1 (impar y mayor que 1)
    if ((e[e.size() - 1] & 1) == 0 || (e.size() == 1 && e[0] == 1) || !menor(e, n))
        return false;
    if ((n[n.size() - 1] & 1) == 0)
        return false;
    if (signature.size() != n.size())
        return false;
    br_rsa_public_key clave {
        const_cast<unsigned char*>(n.data()), n.size(),
        const_cast<unsigned char*>(e.data()), e.size()
    };
    auto resumen = Hash::digest(hash_kind, message);
    unsigned char de_la_firma[64];
    auto comprueba = br_rsa_pkcs1_vrfy_get_default();
    if (!comprueba(signature.data(), signature.size(), oid_de(hash_kind), resumen.size, &clave, de_la_firma))
        return false;
    // (en tiempo constante: no dice dónde deja de coincidir)
    unsigned char d = 0;
    for (size_t i = 0; i < resumen.size; i++)
        d |= de_la_firma[i] ^ resumen.data[i];
    return d == 0;
}

}
