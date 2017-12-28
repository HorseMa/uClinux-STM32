/*
* --------------------------------------------------------------------
* Copyright c                  Realtek Semiconductor Corporation, 2003  
* All rights reserved.
* 
* Program : Testing code for 8651B SHA-1 verification
* Abstract : 
* $Id: sha1test.c,v 1.1.2.1 2007/09/28 14:42:25 davidm Exp $
* $Log: sha1test.c,v $
* Revision 1.1.2.1  2007/09/28 14:42:25  davidm
* #12420
*
* Pull in all the appropriate networking files for the RTL8650,  try to
* clean it up a best as possible so that the files for the build are not
* all over the tree.  Still lots of crazy dependencies in there though.
*
* Revision 1.2  2004/06/23 10:15:45  yjlou
* *: convert DOS format to UNIX format
*
* Revision 1.1  2004/06/23 09:18:57  yjlou
* +: support 865xB CRYPTO Engine
*   +: CONFIG_RTL865XB_EXP_CRYPTOENGINE
*   +: basic encry/decry functions (DES/3DES/SHA1/MAC)
*   +: old-fashion API (should be removed in next version)
*   +: batch functions (should be removed in next version)
*
* Revision 1.4  2003/09/23 02:19:05  jzchen
* Test generic simulation API
*
* Revision 1.3  2003/09/08 09:20:53  jzchen
* Test SHA1 hashing simulator function
*
* Revision 1.2  2003/09/01 02:51:09  jzchen
* Porting test code for MD5 and SHA-1. Test software code ready.
*
*
* --------------------------------------------------------------------
*//* crypto/sha/sha1test.c */
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

#include <rtl_glue.h>
#include <sha1.h>
#include <authSim.h>

#define  SHA_1 /* FIPS 180-1 */

static char *test[]={
	"abc",
	"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
	NULL,
	};

static char *ret[]={
	"a9993e364706816aba3e25717850c26c9cd0d89d",
	"84983e441c3bd26ebaae4aa1f95129e5e54670f1",
	};
static char *bigret=
	"34aa973cd4c4daa4f61eeb2bdbad27316534016f";

static char *pt(unsigned char *md) {
	int i;
	static char buf[80];

	for (i=0; i<SHA_DIGEST_LENGTH; i++)
		sprintf(&(buf[i*2]),"%02x",md[i]);
	return(buf);
}

static uint8 sha1TestData[256];

int sha1test(void) {
	int i,err=0;
	unsigned char **P,**R;
	static unsigned char buf[1000];
	char *p,*r;
	SHA1_CTX c;
	unsigned char md[SHA_DIGEST_LENGTH];

	P=(unsigned char **)test;
	R=(unsigned char **)ret;
	i=1;
	while (*P != NULL)
		{
		SHA1Init(&c);
		SHA1Update(&c, *P, (unsigned long)strlen((char *)*P));
		SHA1Final(md, &c);
		p=pt(md);
		if (strcmp(p,(char *)*R) != 0) {
			rtlglue_printf("error calculating SHA1 on '%s'\n",*P);
			rtlglue_printf("got %s instead of %s\n",p,*R);
			err++;
		}
		else
			rtlglue_printf("test %d ok\n",i);
		memcpy(sha1TestData, *P, (unsigned long)strlen((char *)*P));
		if(authSim_sha1(sha1TestData, (unsigned long)strlen((char *)*P), md) == FAILED)
			rtlglue_printf("SHA1 simulator execution failed\n");
		p=pt(md);
		if (strcmp(p,(char *)*R) != 0) {
			rtlglue_printf("error simulator calculating SHA1 on '%s'\n",*P);
			rtlglue_printf("got %s instead of %s\n",p,*R);
			err++;
		}
		else
			rtlglue_printf("SHA1 simulator test %d ok\n",i);

		memcpy(sha1TestData, *P, (unsigned long)strlen((char *)*P));
		if(authSim(SWHASH_SHA1, sha1TestData, (unsigned long)strlen((char *)*P), NULL, 0, md) == FAILED)
			rtlglue_printf("SHA1 simulator 2 execution failed\n");
		p=pt(md);
		if (strcmp(p,(char *)*R) != 0) {
			rtlglue_printf("error simulator 2 calculating SHA1 on '%s'\n",*P);
			rtlglue_printf("got %s instead of %s\n",p,*R);
			err++;
		}
		else
			rtlglue_printf("SHA1 simulator 2 test %d ok\n",i);

		i++;
		R++;
		P++;
	}

	memset(buf,'a',1000);
	SHA1Init(&c);
	for (i=0; i<1000; i++)
		SHA1Update(&c,buf,1000);
	SHA1Final(md,&c);
	p=pt(md);

	r=bigret;
	if (strcmp(p,r) != 0) {
		rtlglue_printf("error calculating SHA1 on 'a' * 1000\n");
		rtlglue_printf("got %s instead of %s\n",p,r);
		err++;
	}
	else
		rtlglue_printf("test 3 ok\n");

	return(0);
}

