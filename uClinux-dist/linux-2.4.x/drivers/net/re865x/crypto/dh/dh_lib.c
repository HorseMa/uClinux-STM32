/* crypto/dh/dh_lib.c */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 * 
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 * 
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from 
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 * 
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */

#ifndef __KERNEL__
#include <stdio.h>
#include "../rtl_cryptGlue.h"
#else
#include "../../rtl865x/types.h"
#endif
#include "../bn/bn.h"
#include "dh.h"

DH *DH_new(void)
{
	return DH_new_method();
}

DH *DH_new_method(void)
	{
	DH_METHOD *meth;
	DH *ret;
	ret=(DH *)rtlglue_malloc(sizeof(DH));

	if (ret == NULL)
		return(NULL);

	ret->meth = DH_OpenSSL();
	meth = ret->meth;
	ret->pad=0;
	ret->version=0;
	ret->p=NULL;
	ret->g=NULL;
	ret->length=0;
	ret->pub_key=NULL;
	ret->priv_key=NULL;
	ret->q=NULL;
	ret->j=NULL;
	ret->seed = NULL;
	ret->seedlen = 0;
	ret->counter = NULL;
	ret->method_mont_p=NULL;
	ret->references = 1;
	ret->flags=meth->flags;
	return(ret);
	}

void DH_free(DH *r)
	{
	DH_METHOD *meth;
	int i;
	if(r == NULL) return;

	if (r->p != NULL) BN_clear_free(r->p);
	if (r->g != NULL) BN_clear_free(r->g);
	if (r->q != NULL) BN_clear_free(r->q);
	if (r->j != NULL) BN_clear_free(r->j);
	if (r->seed) rtlglue_free(r->seed);
	if (r->counter != NULL) BN_clear_free(r->counter);
	if (r->pub_key != NULL) BN_clear_free(r->pub_key);
	if (r->priv_key != NULL) BN_clear_free(r->priv_key);
	rtlglue_free(r);
	}

int DH_size(DH *dh)
	{
	return(BN_num_bytes(dh->p));
	}

