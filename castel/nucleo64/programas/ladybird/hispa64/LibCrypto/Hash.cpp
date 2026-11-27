/*
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * nucleo64/programas/ladybird/hispa64/LibCrypto/Hash.cpp - Hispa-64, etapa 10 f:
 * LibCrypto/Hash/Hash.h de Ladybird (la misma interfaz), con BearSSL (SHA-1 y
 * SHA-2) y sha3.c (SHA-3) en vez de OpenSSL. El contexto (EVP_MD_CTX en la
 * cabecera de Ladybird: un puntero opaco) es aquí el nuestro.
 */

#include <LibCrypto/Hash/Hash.h>

#include <bearssl.h>
#include "sha3.h"

struct evp_md_ctx_st {
    Crypto::Hash::HashKind kind;
    br_hash_compat_context br;
    hispa_sha3 sha3;
};

namespace Crypto::Hash {

static_assert(sizeof(Digest::data) == 64);

// (el nombre que da OpenSSL a cada uno: Ladybird lo usa también para decirlo)
ErrorOr<char const*> openssl_digest_name(HashKind kind)
{
    switch (kind) {
    case HashKind::SHA1:
        return "SHA1";
    case HashKind::SHA256:
        return "SHA256";
    case HashKind::SHA384:
        return "SHA384";
    case HashKind::SHA512:
        return "SHA512";
    case HashKind::SHA3_256:
        return "SHA3-256";
    case HashKind::SHA3_384:
        return "SHA3-384";
    case HashKind::SHA3_512:
        return "SHA3-512";
    case HashKind::Unknown:
    case HashKind::None:
        break;
    }
    return Error::from_string_literal("Unsupported hash kind");
}

size_t digest_size(HashKind kind)
{
    switch (kind) {
    case HashKind::SHA1:
        return 20;
    case HashKind::SHA256:
    case HashKind::SHA3_256:
        return 32;
    case HashKind::SHA384:
    case HashKind::SHA3_384:
        return 48;
    case HashKind::SHA512:
    case HashKind::SHA3_512:
        return 64;
    case HashKind::Unknown:
    case HashKind::None:
        break;
    }
    VERIFY_NOT_REACHED();
}

static br_hash_class const* clase_de_bearssl(HashKind kind)
{
    switch (kind) {
    case HashKind::SHA1:
        return &br_sha1_vtable;
    case HashKind::SHA256:
        return &br_sha256_vtable;
    case HashKind::SHA384:
        return &br_sha384_vtable;
    case HashKind::SHA512:
        return &br_sha512_vtable;
    default:
        return nullptr;
    }
}

static bool es_sha3(HashKind kind)
{
    return kind == HashKind::SHA3_256 || kind == HashKind::SHA3_384 || kind == HashKind::SHA3_512;
}

Digest digest(HashKind kind, ReadonlyBytes data)
{
    Hasher hasher(kind);
    hasher.update(data);
    return hasher.finish();
}

Hasher::Hasher(HashKind kind)
    : m_context(new evp_md_ctx_st)
{
    // (como OpenSSL: con uno que no sabe, no sigue)
    MUST(openssl_digest_name(kind));
    m_context->kind = kind;
    if (es_sha3(kind))
        hispa_sha3_init(&m_context->sha3, digest_size(kind));
    else
        clase_de_bearssl(kind)->init(&m_context->br.vtable);
}

Hasher::~Hasher()
{
    delete m_context;
}

void Hasher::update(ReadonlyBytes data)
{
    if (es_sha3(m_context->kind))
        hispa_sha3_update(&m_context->sha3, data.data(), data.size());
    else
        m_context->br.vtable->update(&m_context->br.vtable, data.data(), data.size());
}

Digest Hasher::finish()
{
    Digest result;
    if (es_sha3(m_context->kind))
        hispa_sha3_final(&m_context->sha3, result.data.data());
    else
        m_context->br.vtable->out(&m_context->br.vtable, result.data.data());
    result.size = digest_size(m_context->kind);
    return result;
}

}
