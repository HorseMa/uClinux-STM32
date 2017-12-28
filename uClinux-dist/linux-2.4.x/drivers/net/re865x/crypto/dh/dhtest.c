/* crypto/dh/dhtest.c */
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/times.h>
#ifndef HZ
#define HZ 100.0
#endif

#include "../rtl_cryptGlue.h"
#include "../bn/bn.h"

#include "dh.h"

static void cb(int p, int n, void *arg);

static const char rnd_seed[] = "string to make the random number generator think it has entropy";

static double Time_F(int s);
#define START	0
#define STOP	1

static double Time_F(int s)
	{
	double ret;
	static struct tms tstart,tend;

	if (s == START)
		{
		times(&tstart);
		return(0);
		}
	else
		{
		times(&tend);
		ret=((double)(tend.tms_utime-tstart.tms_utime))/HZ;
		return((ret < 1e-3)?1e-3:ret);
		}
	}

int main(int argc, char *argv[])
	{
	DH *a;
	DH *b=NULL;
	char buf[12];
	unsigned char *abuf=NULL,*bbuf=NULL;
	int i,alen,blen,aout,bout,ret=1;
	double tm;

	Time_F(START);
	a=DH_generate_parameters(64,DH_GENERATOR_5,cb,NULL);
	tm=Time_F(STOP);
	printf("DH_generate_parameters %8.3fms\n",tm*1000.0);

	if (a == NULL) goto err;

	if (!DH_check(a, &i)) goto err;
	if (i & DH_CHECK_P_NOT_PRIME)
		printf( "p value is not prime\n");
	if (i & DH_CHECK_P_NOT_SAFE_PRIME)
		printf( "p value is not a safe prime\n");
	if (i & DH_UNABLE_TO_CHECK_GENERATOR)
		printf( "unable to check the generator value\n");
	if (i & DH_NOT_SUITABLE_GENERATOR)
		printf( "the g value is not a generator\n");

	printf("\np    =");
	BN_print(a->p);
	printf("\ng    =");
	BN_print(a->g);
	printf("\n");

	b=DH_new();
	if (b == NULL) goto err;

	b->p=BN_dup(a->p);
	b->g=BN_dup(a->g);
	if ((b->p == NULL) || (b->g == NULL)) goto err;

	Time_F(START);
	if (!DH_generate_key(a)) goto err;
	tm=Time_F(STOP);
	printf("DH_generate_key-a %8.3fms\n",tm*1000.0);
	printf("pri 1=");
	BN_print(a->priv_key);
	printf("\npub 1=");
	BN_print(a->pub_key);
	printf("\n");

	Time_F(START);
	if (!DH_generate_key(b)) goto err;
	tm=Time_F(STOP);
	printf("DH_generate_key-b %8.3fms\n",tm*1000.0);
	printf("pri 2=");
	BN_print(b->priv_key);
	printf("\npub 2=");
	BN_print(b->pub_key);
	printf("\n");

	alen=DH_size(a);
	abuf=(unsigned char *)rtlglue_malloc(alen);
	Time_F(START);
	aout=DH_compute_key(abuf,b->pub_key,a);
	tm=Time_F(STOP);
	printf("DH_compute_key-b.pub,a %8.3fms\n",tm*1000.0);

	printf("key1 =");
	for (i=0; i<aout; i++)
		{
		sprintf(buf,"%02X",abuf[i]);
		printf(buf);
		}
	printf("\n");

	blen=DH_size(b);
	bbuf=(unsigned char *)rtlglue_malloc(blen);
	Time_F(START);
	bout=DH_compute_key(bbuf,a->pub_key,b);
	tm=Time_F(STOP);
	printf("DH_compute_key-a.pub,b %8.3fms\n",tm*1000.0);

	printf("key2 =");
	for (i=0; i<bout; i++)
		{
		sprintf(buf,"%02X",bbuf[i]);
		printf(buf);
		}
	printf("\n");
	if ((aout < 4) || (bout != aout) || (memcmp(abuf,bbuf,aout) != 0))
		{
		printf("Error in DH routines\n");
		ret=1;
		}
	else
		ret=0;
err:

	if (abuf != NULL) rtlglue_free(abuf);
	if (bbuf != NULL) rtlglue_free(bbuf);
	if(b != NULL) DH_free(b);
	if(a != NULL) DH_free(a);
	return(ret);
	}

static void cb(int p, int n, void *arg)
	{
	char c='*';

	if (p == 0) c='.';
	if (p == 1) c='+';
	if (p == 2) c='*';
	if (p == 3) c='\n';
	printf("%c",c);
	}

