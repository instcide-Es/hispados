/*
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * hispa64/RequestServer/CertificateExceptions.cpp - Hispa-64, etapa 10 g
 * (docs/64/HISPA-64.md, sección 24): las excepciones de certificados de
 * RequestServer (las que da el usuario a uno que no pasa), sin OpenSSL: con
 * BearSSL no se ve el certificado rechazado, así que no hay huella ni
 * excepciones (un certificado que no pasa, no pasa).
 */
#include <RequestServer/CertificateExceptions.h>

namespace RequestServer {

ByteString certificate_fingerprint(X509*) { return {}; }
void add_certificate_exception(ByteString const&, u16, ByteString, Requests::CertificateError, ByteString) { }
bool has_certificate_exception(ByteString const&, u16, ByteString const&, Requests::CertificateError, StringView) { return false; }
bool has_any_certificate_exception(ByteString const&, u16) { return false; }

}
