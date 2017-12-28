/*
* --------------------------------------------------------------------
* Copyright c                  Realtek Semiconductor Corporation, 2003  
* All rights reserved.
* 
* Program : Testing code for 8651B MD5 verification
* Abstract : 
* $Id: md5test.c,v 1.1.2.1 2007/09/28 14:42:25 davidm Exp $
* $Log: md5test.c,v $
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
* Revision 1.4  2003/09/23 02:20:16  jzchen
* Test generic simulation API
*
* Revision 1.3  2003/09/08 09:21:42  jzchen
* Add MD5 hashing simulator function test
*
* Revision 1.2  2003/09/01 02:51:09  jzchen
* Porting test code for MD5 and SHA-1. Test software code ready.
*
*
* --------------------------------------------------------------------
*/
/* crypto/md5/md5test.c */
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

#include <md5.h>
#include <authSim.h>
#include <rtl_glue.h>

static char *test[]={
	"",
	"a",
	"abc",
	"message digest",
	"abcdefghijklmnopqrstuvwxyz",
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
	"12345678901234567890123456789012345678901234567890123456789012345678901234567890",
	NULL,
	};

static char *ret[]={
	"d41d8cd98f00b204e9800998ecf8427e",
	"0cc175b9c0f1b6a831c399e269772661",
	"900150983cd24fb0d6963f7d28e17f72",
	"f96b697d7cb7938d525a2f31aaf161d0",
	"c3fcd3d76192e4007dfb496cca67e13b",
	"d174ab98d277d9f5a5611c2c9f419d9f",
	"57edf4a22be3c955ac49da2e2107b67a",
	};

static char *pt(unsigned char *md) {
	int i;
	static char buf[80];

	for (i=0; i<MD5_DIGEST_LENGTH; i++)
		sprintf(&(buf[i*2]),"%02x",md[i]);
	return(buf);
}

uint8 md5TestData[256];

int md5test(void ) {
	int i,err=0;
	unsigned char **P,**R;
	char *p;
	unsigned char md[MD5_DIGEST_LENGTH];
	MD5_CTX md5Ctrl;

	P=(unsigned char **)test;
	R=(unsigned char **)ret;
	i=1;
	while (*P != NULL) {
		MD5Init(&md5Ctrl);
		MD5Update(&md5Ctrl, &(P[0][0]), (unsigned long)strlen((char *)*P));
		MD5Final(&md[0], &md5Ctrl);
		
		p=pt(md);
		if (strcmp(p,(char *)*R) != 0) {
			rtlglue_printf("error calculating MD5 on '%s'\n",*P);
			rtlglue_printf("got %s instead of %s\n",p,*R);
			err++;
		}
		else
			rtlglue_printf("test %d ok\n",i);

		memcpy(md5TestData, &(P[0][0]), (unsigned long)strlen((char *)*P));
		if(authSim_md5(md5TestData, (unsigned long)strlen((char *)*P), &md[0]) == FAILED)
			rtlglue_printf("MD5 simulator execution failed\n");
		p=pt(md);
		if (strcmp(p,(char *)*R) != 0) {
			rtlglue_printf("MD5 simulator error calculating MD5 on '%s'\n",*P);
			rtlglue_printf("got %s instead of %s\n",p,*R);
			err++;
		}
		else
			rtlglue_printf("MD5 simulator test %d ok\n",i);

		memcpy(md5TestData, &(P[0][0]), (unsigned long)strlen((char *)*P));
		if(authSim(SWHASH_MD5, md5TestData, (unsigned long)strlen((char *)*P), NULL, 0, &md[0]) == FAILED)
			rtlglue_printf("MD5 simulator 2 execution failed\n");
		p=pt(md);
		if (strcmp(p,(char *)*R) != 0) {
			rtlglue_printf("MD5 simulator 2 error calculating MD5 on '%s'\n",*P);
			rtlglue_printf("got %s instead of %s\n",p,*R);
			err++;
		}
		else
			rtlglue_printf("MD5 simulator 2 test %d ok\n",i);

		i++;
		R++;
		P++;
	}
	return(0);
}

