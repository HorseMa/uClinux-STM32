/* crypto/bn/bn_div.c */
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
#else
#include "../../rtl865x/types.h"
#endif
#include "bn.h"
#include "bn_lcl.h"

/* The old slow way */

int BN_div(BIGNUM *dv, BIGNUM *rm, const BIGNUM *num, const BIGNUM *divisor,
	   BN_CTX *ctx)
	{
	int norm_shift,i,j,loop;
	BIGNUM *tmp,wnum,*snum,*sdiv,*res;
	BN_ULONG *resp,*wnump;
	BN_ULONG d0,d1;
	int num_n,div_n;

	bn_check_top(num);
	bn_check_top(divisor);

	if (BN_is_zero(divisor))
		return(0);

	if (BN_ucmp(num,divisor) < 0)
		{
		if (rm != NULL)
			{ if (BN_copy(rm,num) == NULL) return(0); }
		if (dv != NULL) BN_zero(dv);
		return(1);
		}

	BN_CTX_start(ctx);
	tmp=BN_CTX_get(ctx);
	snum=BN_CTX_get(ctx);
	sdiv=BN_CTX_get(ctx);
	if (dv == NULL)
		res=BN_CTX_get(ctx);
	else	res=dv;
	if (sdiv==NULL || res == NULL) goto err;
	tmp->neg=0;

	/* First we normalise the numbers */
	norm_shift=BN_BITS2-((BN_num_bits(divisor))%BN_BITS2);
	if (!(BN_lshift(sdiv,divisor,norm_shift))) goto err;
	sdiv->neg=0;
	norm_shift+=BN_BITS2;
	if (!(BN_lshift(snum,num,norm_shift))) goto err;
	snum->neg=0;
	div_n=sdiv->top;
	num_n=snum->top;
	loop=num_n-div_n;

	/* Lets setup a 'window' into snum
	 * This is the part that corresponds to the current
	 * 'area' being divided */
	BN_init(&wnum);
	wnum.d=	 &(snum->d[loop]);
	wnum.top= div_n;
	wnum.dmax= snum->dmax+1; /* a bit of a lie */

	/* Get the top 2 words of sdiv */
	/* i=sdiv->top; */
	d0=sdiv->d[div_n-1];
	d1=(div_n == 1)?0:sdiv->d[div_n-2];

	/* pointer to the 'top' of snum */
	wnump= &(snum->d[num_n-1]);

	/* Setup to 'res' */
	res->neg= (num->neg^divisor->neg);
	if (!bn_wexpand(res,(loop+1))) goto err;
	res->top=loop;
	resp= &(res->d[loop-1]);

	/* space for temp */
	if (!bn_wexpand(tmp,(div_n+1))) goto err;

	if (BN_ucmp(&wnum,sdiv) >= 0)
		{
		if (!BN_usub(&wnum,&wnum,sdiv)) goto err;
		*resp=1;
		res->d[res->top-1]=1;
		}
	else
		res->top--;
	resp--;

	for (i=0; i<loop-1; i++)
		{
		BN_ULONG q,l0;
		BN_ULONG n0,n1,rem=0;

		n0=wnump[0];
		n1=wnump[-1];
		if (n0 == d0)
			q=BN_MASK2;
		else 			/* n0 < d0 */
			{
			BN_ULONG t2l,t2h,ql,qh;

			q=bn_div_words(n0,n1,d0);
			rem=(n1-q*d0)&BN_MASK2;

			t2l=LBITS(d1); t2h=HBITS(d1);
			ql =LBITS(q);  qh =HBITS(q);
			mul64(t2l,t2h,ql,qh); /* t2=(BN_ULLONG)d1*q; */

			for (;;)
				{
				if ((t2h < rem) ||
					((t2h == rem) && (t2l <= wnump[-2])))
					break;
				q--;
				rem += d0;
				if (rem < d0) break; /* don't let rem overflow */
				if (t2l < d1) t2h--; t2l -= d1;
				}
			}

		l0=bn_mul_words(tmp->d,sdiv->d,div_n,q);
		wnum.d--; wnum.top++;
		tmp->d[div_n]=l0;
		for (j=div_n+1; j>0; j--)
			if (tmp->d[j-1]) break;
		tmp->top=j;

		j=wnum.top;
		if (!BN_sub(&wnum,&wnum,tmp)) goto err;

		snum->top=snum->top+wnum.top-j;

		if (wnum.neg)
			{
			q--;
			j=wnum.top;
			if (!BN_add(&wnum,&wnum,sdiv)) goto err;
			snum->top+=wnum.top-j;
			}
		*(resp--)=q;
		wnump--;
		}
	if (rm != NULL)
		{
		BN_rshift(rm,snum,norm_shift);
		rm->neg=num->neg;
		}
	BN_CTX_end(ctx);
	return(1);
err:
	BN_CTX_end(ctx);
	return(0);
	}


/* rem != m */
int BN_mod(BIGNUM *rem, const BIGNUM *m, const BIGNUM *d, BN_CTX *ctx)
	{
	return(BN_div(NULL,rem,m,d,ctx));
	}

