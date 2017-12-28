/* 
 * Interface to the Open Cryptographic Framework (OCF) 
 * Daniel Djamaludin <ddjamaludin@cyberguard.com>
 * Copyright (C) 2004-2005 Intel Corporation.  All Rights Reserved.
 *
 * The code was developed with source from the openssl package,
 * file: hw_cryptodev.c
 *
 * Copyright (c) 2002 Bob Beck <beck@openbsd.org>
 * Copyright (c) 2002 Theo de Raadt
 * Copyright (c) 2002 Markus Friedl
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/* yuck */
#define NO_ASN1_TYPEDEFS 1
#include <openssl/bn.h>
/* yuck yuck !*/
#undef NO_ASN1_TYPEDEFS
#undef ASN1_INTEGER
#undef ASN1_ENUMERATED
#undef ASN1_BIT_STRING
#undef ASN1_OCTET_STRING
#undef ASN1_PRINTABLESTRING
#undef ASN1_T61STRING
#undef ASN1_IA5STRING
#undef ASN1_UTCTIME
#undef ASN1_GENERALIZEDTIME
#undef ASN1_TIME
#undef ASN1_GENERALSTRING
#undef ASN1_UNIVERSALSTRING
#undef ASN1_BMPSTRING
#undef ASN1_VISIBLESTRING
#undef ASN1_UTF8STRING
#undef ASN1_BOOLEAN
#undef ASN1_NULL

struct cryptodev_meth {
	int (*rsa_mod_exp_crt)(const struct RSA_private_key *k, mpz_t *t1, BIGNUM *r0);
	int (*mod_exp)(BIGNUM *r0, MP_INT *mp_g, MP_INT *secret, MP_INT *modulus);
};

extern struct cryptodev_meth cryptodev;

extern void load_cryptodev(void);
extern int bn2mp(const BIGNUM *a, MP_INT *mp);
