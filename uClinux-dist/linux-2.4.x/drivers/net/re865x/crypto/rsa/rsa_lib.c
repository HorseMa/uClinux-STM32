/* crypto/rsa/rsa_lib.c */
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
#include "rsa.h"

static RSA_METHOD *default_RSA_meth=NULL;
static int rsa_meth_num=0;

RSA *RSA_new(void)
	{
	RSA *r=RSA_new_method(NULL);

	return r;
	}


RSA *RSA_new_method(RSA_METHOD *meth)
	{
	RSA *ret;

	ret=(RSA *)rtlglue_malloc(sizeof(RSA));
	if (ret == NULL)
		return(NULL);

	if (meth == NULL)
		{
		if((ret->meth=RSA_PKCS1_SSLeay()) == NULL)
			{
			rtlglue_free(ret);
			return NULL;
			}
		}
	else
		ret->meth=meth;
	meth = ret->meth;

	ret->pad=0;
	ret->version=0;
	ret->n=NULL;
	ret->e=NULL;
	ret->d=NULL;
	ret->p=NULL;
	ret->q=NULL;
	ret->dmp1=NULL;
	ret->dmq1=NULL;
	ret->iqmp=NULL;
	ret->references=1;
	ret->_method_mod_n=NULL;
	ret->_method_mod_p=NULL;
	ret->_method_mod_q=NULL;
	ret->blinding=NULL;
	ret->bignum_data=NULL;
	ret->flags=meth->flags;
	if ((meth->init != NULL) && !meth->init(ret))
		{
		rtlglue_free(ret);
		ret=NULL;
		}
	return(ret);
	}

void RSA_free(RSA *r)
	{
	RSA_METHOD *meth;

	if (r == NULL) return;


	meth = r->meth;
	if (meth->finish != NULL)
		meth->finish(r);

	if (r->n != NULL) BN_clear_free(r->n);
	if (r->e != NULL) BN_clear_free(r->e);
	if (r->d != NULL) BN_clear_free(r->d);
	if (r->p != NULL) BN_clear_free(r->p);
	if (r->q != NULL) BN_clear_free(r->q);
	if (r->dmp1 != NULL) BN_clear_free(r->dmp1);
	if (r->dmq1 != NULL) BN_clear_free(r->dmq1);
	if (r->iqmp != NULL) BN_clear_free(r->iqmp);
	if (r->blinding != NULL) BN_BLINDING_free(r->blinding);
	rtlglue_free(r);
	}

int RSA_size(RSA *r)
	{
	return(BN_num_bytes(r->n));
	}

int RSA_public_encrypt(int flen, unsigned char *from, unsigned char *to,
	     RSA *rsa, int padding)
	{
	return(rsa->meth->rsa_pub_enc(flen,
		from, to, rsa, padding));
	}

int RSA_private_encrypt(int flen, unsigned char *from, unsigned char *to,
	     RSA *rsa, int padding)
	{
	return(rsa->meth->rsa_priv_enc(flen,
		from, to, rsa, padding));
	}

int RSA_private_decrypt(int flen, unsigned char *from, unsigned char *to,
	     RSA *rsa, int padding)
	{
	return(rsa->meth->rsa_priv_dec(flen,
		from, to, rsa, padding));
	}

int RSA_public_decrypt(int flen, unsigned char *from, unsigned char *to,
	     RSA *rsa, int padding)
	{
	return(rsa->meth->rsa_pub_dec(flen,
		from, to, rsa, padding));
	}

int RSA_flags(RSA *r)
	{
	return((r == NULL)?0:r->meth->flags);
	}

void RSA_blinding_off(RSA *rsa)
	{
	if (rsa->blinding != NULL)
		{
		BN_BLINDING_free(rsa->blinding);
		rsa->blinding=NULL;
		}
	rsa->flags &= ~RSA_FLAG_BLINDING;
	rsa->flags |= RSA_FLAG_NO_BLINDING;
	}

int RSA_blinding_on(RSA *rsa, BN_CTX *p_ctx)
	{
	BIGNUM *A,*Ai = NULL;
	BN_CTX *ctx;
	int ret=0;

	if (p_ctx == NULL)
		{
		if ((ctx=BN_CTX_new()) == NULL) goto err;
		}
	else
		ctx=p_ctx;

	/* XXXXX: Shouldn't this be RSA_blinding_off(rsa)? */
	if (rsa->blinding != NULL)
		{
		BN_BLINDING_free(rsa->blinding);
		rsa->blinding = NULL;
		}

	/* NB: similar code appears in setup_blinding (rsa_eay.c);
	 * this should be placed in a new function of its own, but for reasons
	 * of binary compatibility can't */

	BN_CTX_start(ctx);
	A = BN_CTX_get(ctx);
	if (rsa->d != NULL && rsa->d->d != NULL)
		{
		if (!BN_pseudo_rand_range(A,rsa->n)) goto err;
		}
	else
		{
		if (!BN_rand_range(A,rsa->n)) goto err;
		}
	if ((Ai=BN_mod_inverse(NULL,A,rsa->n,ctx)) == NULL) goto err;

	if (!rsa->meth->bn_mod_exp(A,A,
		rsa->e,rsa->n,ctx,rsa->_method_mod_n))
	    goto err;
	if ((rsa->blinding=BN_BLINDING_new(A,Ai,rsa->n)) == NULL) goto err;
	/* to make things thread-safe without excessive locking,
	 * rsa->blinding will be used just by the current thread: */
	rsa->flags |= RSA_FLAG_BLINDING;
	rsa->flags &= ~RSA_FLAG_NO_BLINDING;
	ret=1;
err:
	if (Ai != NULL) BN_free(Ai);
	BN_CTX_end(ctx);
	if (ctx != p_ctx) BN_CTX_free(ctx);
	return(ret);
	}

