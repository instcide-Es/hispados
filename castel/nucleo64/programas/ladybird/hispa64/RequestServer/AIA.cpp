/*
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * hispa64/RequestServer/AIA.cpp - Hispa-64, etapa 10 g (docs/64/HISPA-64.md,
 * sección 24): lo de AIA.h de RequestServer de Ladybird sin OpenSSL. Ladybird
 * busca los certificados intermedios que le faltan (caIssuers, «Authority
 * Information Access») mientras OpenSSL comprueba la cadena; con BearSSL no
 * hay ese gancho: el servidor tiene que mandar su cadena entera (lo normal), y
 * esto no recoge nada.
 */
#include <RequestServer/AIA.h>

namespace RequestServer {

void apply_aia_verification(SSL_CTX*, AIACollector&) { }
void explain_system_verifier_refusal(AIACollector&) { }
bool add_fetched_aia_intermediates(ReadonlyBytes, ReadonlySpan<NonnullRefPtr<AIACollector>>) { return false; }
void mark_aia_url_failed(IsPrivate, ByteString) { }
void clear_aia_state(IsPrivate) { }
Vector<ByteString> ca_issuers_urls(X509*) { return {}; }
Vector<X509*> parse_certificates(ReadonlyBytes) { return {}; }

}
