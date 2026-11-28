/*
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * hispa64/webcripto/mdctx.h - Hispa-64, etapa 10 i: el EVP_MD_CTX de OpenSSL,
 * que vale para un resumen (resumen.c) y para firmar o comprobar con una
 * clave (EVP_DigestSign, EVP_DigestVerify: claves.c).
 */
#ifndef H64_WEBCRIPTO_MDCTX_H
#define H64_WEBCRIPTO_MDCTX_H
#include "interno.h"
enum { MD_NADA = 0, MD_RESUMEN, MD_FIRMA, MD_COMPRUEBA };
typedef struct evp_md_ctx_st {
	int modo;
	h64_resumen r;
	EVP_PKEY *k;
	int resumen;   /* el de la firma (R_NADA en Ed25519) */
	int relleno;   /* RSA: 1, PKCS #1 v1.5; 2, PSS */
	int mgf1;
	long sal;      /* PSS: la longitud de la sal (-1: la del resumen) */
} EVP_MD_CTX;
void h64_mdctx_suelta(EVP_MD_CTX *c);
#endif
