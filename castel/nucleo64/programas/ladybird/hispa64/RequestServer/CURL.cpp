/*
 * Copyright (c) 2024, Andrew Kaster <andrew@ladybird.org>
 * Copyright (c) 2025, Tim Flynn <trflynn89@ladybird.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Hispa-64, etapa 10 g (docs/64/HISPA-64.md, sección 24): el CURL.cpp de
 * RequestServer de Ladybird (terceros/ladybird/Services/RequestServer), sin
 * OpenSSL: en Hispa-64, curl va con BearSSL (programas/hazcurl.sh), que
 * comprueba él solo la cadena y el nombre (CURLOPT_SSL_VERIFYHOST 2) con las
 * autoridades de CURLOPT_CAINFO; no hay SSL_CTX que tocar (sin
 * CURLOPT_SSL_CTX_FUNCTION), ni AIA, ni excepciones de certificados. Lo
 * demás, tal cual.
 * Los cambios: Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o
 * posterior.
 */

#include <AK/ByteString.h>
#include <AK/Enumerate.h>
#include <AK/Format.h>
#include <RequestServer/AIA.h>
#include <RequestServer/CURL.h>
#include <RequestServer/Resolver.h>
#include <string.h>

namespace RequestServer {

ErrorOr<void> initialize_libcurl()
{
    auto result = curl_global_init(CURL_GLOBAL_ALL);
    if (result != CURLE_OK) {
        warnln("curl_global_init failed: {}", curl_easy_strerror(result));
        return Error::from_string_literal("Failed to initialize libcurl");
    }
    return {};
}

CURLcode configure_certificate_verification(CURL* handle, AIACollector&)
{
    // (BearSSL: curl comprueba la cadena y el nombre)
    if (auto result = curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 1L); result != CURLE_OK)
        return result;
    return curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, 2L);
}

CURLcode configure_ssl_context(CURL*, void*, void*)
{
    return CURLE_OK;
}

bool can_pin_host_in_curl_resolve_list(StringView host)
{
    return !host.is_empty() && host[0] != '-' && host[0] != '+';
}

ByteString build_curl_resolve_list(DNS::LookupResult const& dns_result, StringView host, u16 port)
{
    StringBuilder resolve_opt_builder;
    resolve_opt_builder.appendff("{}:{}:", host, port);

    for (auto const& [i, addr] : enumerate(dns_result.cached_addresses())) {
        auto formatted_address = addr.visit(
            [&](IPv4Address const& ipv4) { return ipv4.to_byte_string(); },
            [&](IPv6Address const& ipv6) { return MUST(ipv6.to_string()).to_byte_string(); });

        if (i > 0)
            resolve_opt_builder.append(',');
        resolve_opt_builder.append(formatted_address);
    }

    dbgln_if(REQUESTSERVER_DEBUG, "RequestServer: Resolve list: {}", resolve_opt_builder.string_view());
    return resolve_opt_builder.to_byte_string();
}

Optional<ByteString> build_curl_connect_to_entry(DNS::LookupResult const& dns_result, StringView host, u16 port, u32 address_index)
{
    auto addresses = dns_result.cached_addresses();
    if (addresses.size() < 2)
        return {};

    auto formatted_address = addresses[address_index % addresses.size()].visit(
        [&](IPv4Address const& ipv4) { return ipv4.to_byte_string(); },
        // CURLOPT_CONNECT_TO, unlike CURLOPT_RESOLVE, parses its host field, so an IPv6 address has to be bracketed
        // for its colons not to read as a port separator.
        [&](IPv6Address const& ipv6) { return ByteString::formatted("[{}]", MUST(ipv6.to_string())); });

    auto entry = ByteString::formatted("{}:{}:{}:{}", host, port, formatted_address, port);
    dbgln_if(REQUESTSERVER_DEBUG, "RequestServer: Connect-to entry: {}", entry);

    return entry;
}

Requests::NetworkError curl_code_to_network_error(int code)
{
    switch (code) {
    case CURLE_COULDNT_RESOLVE_HOST:
        return Requests::NetworkError::UnableToResolveHost;
    case CURLE_COULDNT_RESOLVE_PROXY:
        return Requests::NetworkError::UnableToResolveProxy;
    case CURLE_COULDNT_CONNECT:
        return Requests::NetworkError::UnableToConnect;
    case CURLE_OPERATION_TIMEDOUT:
        return Requests::NetworkError::TimeoutReached;
    case CURLE_TOO_MANY_REDIRECTS:
        return Requests::NetworkError::TooManyRedirects;
    case CURLE_SSL_CONNECT_ERROR:
        return Requests::NetworkError::SSLHandshakeFailed;
    case CURLE_PEER_FAILED_VERIFICATION:
        return Requests::NetworkError::SSLVerificationFailed;
    case CURLE_URL_MALFORMAT:
        return Requests::NetworkError::MalformedUrl;
    case CURLE_PARTIAL_FILE:
        return Requests::NetworkError::IncompleteContent;
    case CURLE_BAD_CONTENT_ENCODING:
        return Requests::NetworkError::InvalidContentEncoding;
    default:
        return Requests::NetworkError::Unknown;
    }
}

}
