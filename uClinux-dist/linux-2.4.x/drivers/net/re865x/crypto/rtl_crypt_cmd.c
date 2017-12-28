#include "../rtl865x/types.h"
#include "../cle/rtl_cle.h" 
#include "../rtl865x/rtl_glue.h" 
#include <linux/string.h>
#include "bn/bn.h"
#include "rsa/rsa.h"
#include "dh/dh.h"

static unsigned char ptext[1024];
static unsigned char ctext[1024];
static unsigned char ctext_ex[1024];


static void cb(int p, int n, void *arg)
	{
	char c='*';

	if (p == 0) c='.';
	if (p == 1) c='+';
	if (p == 2) c='*';
	if (p == 3) c='\n';
	rtlglue_printf("%c",c);
	}

static int32	_rtl865x_perfCryptCmd(uint32 userId,  int32 argc,int8 **saved){
	int32 size;
	uint32 bits;
	int8 *nextToken;
	
	cle_getNextCmdToken(&nextToken,&size,saved); 
	if(!strcmp(nextToken, "rsa")) {
		RSA *key;
		static unsigned char ptext_ex[] = "\x54\x85";
		int32 plen, clen = 0, dlen;
		uint32 time1, time2, i, testRound;

		cle_getNextCmdToken(&nextToken,&size,saved); 
		bits = U32_value(nextToken);
		plen = sizeof(ptext_ex) - 1;
		
		testRound = 1;
		time1 = time2 = 0;
		while(time2-time1<100) {			
			time1 = getmstime();
			for(i=0; i<testRound; i++) {
				key = RSA_generate_key(bits, 11, NULL, NULL);
				RSA_free(key);
			}
			time2 = getmstime();
			testRound <<= 1;
		}
		testRound >>= 1;
		rtlglue_printf("RSA Generate %d-bit key need %ums%03uus operate %d times\n", bits, (time2-time1)/testRound, ((time2-time1)*1000/testRound)%1000, testRound);

		key = RSA_generate_key(bits, 11, NULL, NULL);

		testRound = 1;
		time1 = time2 = 0;
		while(time2-time1<100) {			
			time1 = getmstime();
			for(i=0; i<testRound; i++) 
				clen = RSA_public_encrypt(plen, ptext_ex, ctext, key, RSA_PKCS1_PADDING);
			time2 = getmstime();
			if(clen<0)
				break;//Error
			testRound <<= 1;
		}
		testRound >>= 1;
		if(clen>0)
			rtlglue_printf("RSA encrypt with %d-bit key need %ums%03uus operate %u times output %u-bits\n", bits, (time2-time1)/testRound, ((time2-time1)*1000/testRound)%1000, testRound, clen<<3);
		else {
			rtlglue_printf("RSA encryption failed clen = %d\n");
			return SUCCESS;
		}
		

		testRound = 1;
		time1 = time2 = 0;
		while(time2-time1<100) {			
			time1 = getmstime();
			for(i=0; i<testRound; i++) 
				dlen = RSA_private_decrypt(clen, ctext, ptext, key, RSA_PKCS1_PADDING);
			time2 = getmstime();
			if(dlen<0)
				break;//Error
			testRound <<= 1;
		}
		testRound >>= 1;
		if(dlen>0)
			rtlglue_printf("RSA decrypt with %d-bit key need %ums%03uus operate %u times output %u-bits\n", bits, (time2-time1)/testRound, ((time2-time1)*1000/testRound)%1000, testRound, dlen<<3);
		else {
			rtlglue_printf("RSA decryption failed\n");
			return SUCCESS;
		}
		if (dlen != plen || memcmp(ptext, ptext_ex, dlen) != 0)
		    rtlglue_printf("PKCS#1 v1.5 encrypted data cannot decrypt to original one!\n");
		else
		    rtlglue_printf("PKCS #1 v1.5 encryption/decryption ok\n");
		
		RSA_free(key);
	}
	else if(!strcmp(nextToken, "dh")) {
		DH *a;
		DH *b=NULL;
		char buf[12];
		unsigned char *abuf=NULL,*bbuf=NULL;
		int alen,blen,aout,bout,ret=1;
		uint32 time1, time2, i, testRound;

		cle_getNextCmdToken(&nextToken,&size,saved); 
		bits = U32_value(nextToken);

		testRound = 1;
		time1 = time2 = 0;
		while(time2-time1<100) {			
			time1 = getmstime();
			for(i=0; i<testRound; i++) {
				a=DH_generate_parameters(bits,DH_GENERATOR_5, NULL,NULL);
				DH_free(a);
			}
			time2 = getmstime();
			testRound <<= 1;
		}
		testRound >>= 1;
		rtlglue_printf("DH Generate %d-bit parameter need %ums%03uus operate %d times\n", bits, (time2-time1)/testRound, ((time2-time1)*1000/testRound)%1000, testRound);

		a=DH_generate_parameters(bits,DH_GENERATOR_5, NULL,NULL);
		if (a == NULL) goto dherr;

		b=DH_new();
		if (b == NULL) goto dherr;

		b->p=BN_dup(a->p);
		b->g=BN_dup(a->g);
		if ((b->p == NULL) || (b->g == NULL)) goto dherr;

		testRound = 1;
		time1 = time2 = 0;
		while(time2-time1<100) {			
			time1 = getmstime();
			for(i=0; i<testRound; i++) 
				if (!DH_generate_key(a)) goto dherr;
			time2 = getmstime();
			testRound <<= 1;
		}
		testRound >>= 1;
		rtlglue_printf("DH Generate %d-bit key need %ums%03uus operate %d times\n", bits, (time2-time1)/testRound, ((time2-time1)*1000/testRound)%1000, testRound);
		
		rtlglue_printf("pri 1=");
		BN_print(a->priv_key);
		rtlglue_printf("\npub 1=");
		BN_print(a->pub_key);
		rtlglue_printf("\n");

		testRound = 1;
		time1 = time2 = 0;
		while(time2-time1<100) {			
			time1 = getmstime();
			for(i=0; i<testRound; i++) 
				if (!DH_generate_key(b)) goto dherr;
			time2 = getmstime();
			testRound <<= 1;
		}
		testRound >>= 1;
		rtlglue_printf("DH Generate %d-bit key need %ums%03uus operate %d times\n", bits, (time2-time1)/testRound, ((time2-time1)*1000/testRound)%1000, testRound);
		rtlglue_printf("pri 2=");
		BN_print(b->priv_key);
		rtlglue_printf("\npub 2=");
		BN_print(b->pub_key);
		rtlglue_printf("\n");

		alen=DH_size(a);
		abuf=(unsigned char *)rtlglue_malloc(alen);

		testRound = 1;
		time1 = time2 = 0;
		while(time2-time1<100) {			
			time1 = getmstime();
			for(i=0; i<testRound; i++) 
				aout=DH_compute_key(abuf,b->pub_key,a);
			time2 = getmstime();
			testRound <<= 1;
		}
		testRound >>= 1;
		rtlglue_printf("DH compute %d-bit key need %ums%03uus operate %d times\n", bits, (time2-time1)/testRound, ((time2-time1)*1000/testRound)%1000, testRound);

		rtlglue_printf("key1 =");
		for (i=0; i<aout; i++)
			{
			sprintf(buf,"%02X",abuf[i]);
			rtlglue_printf(buf);
			}
		rtlglue_printf("\n");

		blen=DH_size(b);
		bbuf=(unsigned char *)rtlglue_malloc(blen);

		testRound = 1;
		time1 = time2 = 0;
		while(time2-time1<100) {			
			time1 = getmstime();
			for(i=0; i<testRound; i++) 
				bout=DH_compute_key(bbuf,a->pub_key,b);
			time2 = getmstime();
			testRound <<= 1;
		}
		testRound >>= 1;
		rtlglue_printf("DH compute %d-bit key need %ums%03uus operate %d times\n", bits, (time2-time1)/testRound, ((time2-time1)*1000/testRound)%1000, testRound);

		rtlglue_printf("key2 =");
		for (i=0; i<bout; i++)
			{
			sprintf(buf,"%02X",bbuf[i]);
			rtlglue_printf(buf);
			}
		rtlglue_printf("\n");
		if ((aout < 4) || (bout != aout) || (memcmp(abuf,bbuf,aout) != 0))
			rtlglue_printf("Error in DH routines\n");
dherr:
		if (abuf != NULL) rtlglue_free(abuf);
		if (bbuf != NULL) rtlglue_free(bbuf);
		if(b != NULL) DH_free(b);
		if(a != NULL) DH_free(a);
		return SUCCESS;
	}
	return SUCCESS;
}

#define SetKey \
  key->n = BN_bin2bn(n, sizeof(n)-1, key->n); \
  key->e = BN_bin2bn(e, sizeof(e)-1, key->e); \
  key->d = BN_bin2bn(d, sizeof(d)-1, key->d); \
  key->p = BN_bin2bn(p, sizeof(p)-1, key->p); \
  key->q = BN_bin2bn(q, sizeof(q)-1, key->q); \
  key->dmp1 = BN_bin2bn(dmp1, sizeof(dmp1)-1, key->dmp1); \
  key->dmq1 = BN_bin2bn(dmq1, sizeof(dmq1)-1, key->dmq1); \
  key->iqmp = BN_bin2bn(iqmp, sizeof(iqmp)-1, key->iqmp); \
  memcpy(c, ctext_ex, sizeof(ctext_ex) - 1); \
  return (sizeof(ctext_ex) - 1);

static int key1(RSA *key, unsigned char *c)
    {
    static unsigned char n[] =
"\x00\xAA\x36\xAB\xCE\x88\xAC\xFD\xFF\x55\x52\x3C\x7F\xC4\x52\x3F"
"\x90\xEF\xA0\x0D\xF3\x77\x4A\x25\x9F\x2E\x62\xB4\xC5\xD9\x9C\xB5"
"\xAD\xB3\x00\xA0\x28\x5E\x53\x01\x93\x0E\x0C\x70\xFB\x68\x76\x93"
"\x9C\xE6\x16\xCE\x62\x4A\x11\xE0\x08\x6D\x34\x1E\xBC\xAC\xA0\xA1"
"\xF5";

    static unsigned char e[] = "\x11";

    static unsigned char d[] =
"\x0A\x03\x37\x48\x62\x64\x87\x69\x5F\x5F\x30\xBC\x38\xB9\x8B\x44"
"\xC2\xCD\x2D\xFF\x43\x40\x98\xCD\x20\xD8\xA1\x38\xD0\x90\xBF\x64"
"\x79\x7C\x3F\xA7\xA2\xCD\xCB\x3C\xD1\xE0\xBD\xBA\x26\x54\xB4\xF9"
"\xDF\x8E\x8A\xE5\x9D\x73\x3D\x9F\x33\xB3\x01\x62\x4A\xFD\x1D\x51";

    static unsigned char p[] =
"\x00\xD8\x40\xB4\x16\x66\xB4\x2E\x92\xEA\x0D\xA3\xB4\x32\x04\xB5"
"\xCF\xCE\x33\x52\x52\x4D\x04\x16\xA5\xA4\x41\xE7\x00\xAF\x46\x12"
"\x0D";
    
    static unsigned char q[] =
"\x00\xC9\x7F\xB1\xF0\x27\xF4\x53\xF6\x34\x12\x33\xEA\xAA\xD1\xD9"
"\x35\x3F\x6C\x42\xD0\x88\x66\xB1\xD0\x5A\x0F\x20\x35\x02\x8B\x9D"
"\x89";

    static unsigned char dmp1[] =
"\x59\x0B\x95\x72\xA2\xC2\xA9\xC4\x06\x05\x9D\xC2\xAB\x2F\x1D\xAF"
"\xEB\x7E\x8B\x4F\x10\xA7\x54\x9E\x8E\xED\xF5\xB4\xFC\xE0\x9E\x05";

    static unsigned char dmq1[] =
"\x00\x8E\x3C\x05\x21\xFE\x15\xE0\xEA\x06\xA3\x6F\xF0\xF1\x0C\x99"
"\x52\xC3\x5B\x7A\x75\x14\xFD\x32\x38\xB8\x0A\xAD\x52\x98\x62\x8D"
"\x51";

    static unsigned char iqmp[] =
"\x36\x3F\xF7\x18\x9D\xA8\xE9\x0B\x1D\x34\x1F\x71\xD0\x9B\x76\xA8"
"\xA9\x43\xE1\x1D\x10\xB2\x4D\x24\x9F\x2D\xEA\xFE\xF8\x0C\x18\x26";

    static unsigned char ctext_ex[] =
"\x1b\x8f\x05\xf9\xca\x1a\x79\x52\x6e\x53\xf3\xcc\x51\x4f\xdb\x89"
"\x2b\xfb\x91\x93\x23\x1e\x78\xb9\x92\xe6\x8d\x50\xa4\x80\xcb\x52"
"\x33\x89\x5c\x74\x95\x8d\x5d\x02\xab\x8c\x0f\xd0\x40\xeb\x58\x44"
"\xb0\x05\xc3\x9e\xd8\x27\x4a\x9d\xbf\xa8\x06\x71\x40\x94\x39\xd2";

    SetKey;
    }

static int key2(RSA *key, unsigned char *c)
    {
    static unsigned char n[] =
"\x00\xA3\x07\x9A\x90\xDF\x0D\xFD\x72\xAC\x09\x0C\xCC\x2A\x78\xB8"
"\x74\x13\x13\x3E\x40\x75\x9C\x98\xFA\xF8\x20\x4F\x35\x8A\x0B\x26"
"\x3C\x67\x70\xE7\x83\xA9\x3B\x69\x71\xB7\x37\x79\xD2\x71\x7B\xE8"
"\x34\x77\xCF";

    static unsigned char e[] = "\x3";

    static unsigned char d[] =
"\x6C\xAF\xBC\x60\x94\xB3\xFE\x4C\x72\xB0\xB3\x32\xC6\xFB\x25\xA2"
"\xB7\x62\x29\x80\x4E\x68\x65\xFC\xA4\x5A\x74\xDF\x0F\x8F\xB8\x41"
"\x3B\x52\xC0\xD0\xE5\x3D\x9B\x59\x0F\xF1\x9B\xE7\x9F\x49\xDD\x21"
"\xE5\xEB";

    static unsigned char p[] =
"\x00\xCF\x20\x35\x02\x8B\x9D\x86\x98\x40\xB4\x16\x66\xB4\x2E\x92"
"\xEA\x0D\xA3\xB4\x32\x04\xB5\xCF\xCE\x91";

    static unsigned char q[] =
"\x00\xC9\x7F\xB1\xF0\x27\xF4\x53\xF6\x34\x12\x33\xEA\xAA\xD1\xD9"
"\x35\x3F\x6C\x42\xD0\x88\x66\xB1\xD0\x5F";
    
    static unsigned char dmp1[] =
"\x00\x8A\x15\x78\xAC\x5D\x13\xAF\x10\x2B\x22\xB9\x99\xCD\x74\x61"
"\xF1\x5E\x6D\x22\xCC\x03\x23\xDF\xDF\x0B";

    static unsigned char dmq1[] =
"\x00\x86\x55\x21\x4A\xC5\x4D\x8D\x4E\xCD\x61\x77\xF1\xC7\x36\x90"
"\xCE\x2A\x48\x2C\x8B\x05\x99\xCB\xE0\x3F";

    static unsigned char iqmp[] =
"\x00\x83\xEF\xEF\xB8\xA9\xA4\x0D\x1D\xB6\xED\x98\xAD\x84\xED\x13"
"\x35\xDC\xC1\x08\xF3\x22\xD0\x57\xCF\x8D";

    static unsigned char ctext_ex[] =
"\x14\xbd\xdd\x28\xc9\x83\x35\x19\x23\x80\xe8\xe5\x49\xb1\x58\x2a"
"\x8b\x40\xb4\x48\x6d\x03\xa6\xa5\x31\x1f\x1f\xd5\xf0\xa1\x80\xe4"
"\x17\x53\x03\x29\xa9\x34\x90\x74\xb1\x52\x13\x54\x29\x08\x24\x52"
"\x62\x51";

    SetKey;
    }

static int key3(RSA *key, unsigned char *c)
    {
    static unsigned char n[] =
"\x00\xBB\xF8\x2F\x09\x06\x82\xCE\x9C\x23\x38\xAC\x2B\x9D\xA8\x71"
"\xF7\x36\x8D\x07\xEE\xD4\x10\x43\xA4\x40\xD6\xB6\xF0\x74\x54\xF5"
"\x1F\xB8\xDF\xBA\xAF\x03\x5C\x02\xAB\x61\xEA\x48\xCE\xEB\x6F\xCD"
"\x48\x76\xED\x52\x0D\x60\xE1\xEC\x46\x19\x71\x9D\x8A\x5B\x8B\x80"
"\x7F\xAF\xB8\xE0\xA3\xDF\xC7\x37\x72\x3E\xE6\xB4\xB7\xD9\x3A\x25"
"\x84\xEE\x6A\x64\x9D\x06\x09\x53\x74\x88\x34\xB2\x45\x45\x98\x39"
"\x4E\xE0\xAA\xB1\x2D\x7B\x61\xA5\x1F\x52\x7A\x9A\x41\xF6\xC1\x68"
"\x7F\xE2\x53\x72\x98\xCA\x2A\x8F\x59\x46\xF8\xE5\xFD\x09\x1D\xBD"
"\xCB";

    static unsigned char e[] = "\x11";

    static unsigned char d[] =
"\x00\xA5\xDA\xFC\x53\x41\xFA\xF2\x89\xC4\xB9\x88\xDB\x30\xC1\xCD"
"\xF8\x3F\x31\x25\x1E\x06\x68\xB4\x27\x84\x81\x38\x01\x57\x96\x41"
"\xB2\x94\x10\xB3\xC7\x99\x8D\x6B\xC4\x65\x74\x5E\x5C\x39\x26\x69"
"\xD6\x87\x0D\xA2\xC0\x82\xA9\x39\xE3\x7F\xDC\xB8\x2E\xC9\x3E\xDA"
"\xC9\x7F\xF3\xAD\x59\x50\xAC\xCF\xBC\x11\x1C\x76\xF1\xA9\x52\x94"
"\x44\xE5\x6A\xAF\x68\xC5\x6C\x09\x2C\xD3\x8D\xC3\xBE\xF5\xD2\x0A"
"\x93\x99\x26\xED\x4F\x74\xA1\x3E\xDD\xFB\xE1\xA1\xCE\xCC\x48\x94"
"\xAF\x94\x28\xC2\xB7\xB8\x88\x3F\xE4\x46\x3A\x4B\xC8\x5B\x1C\xB3"
"\xC1";

    static unsigned char p[] =
"\x00\xEE\xCF\xAE\x81\xB1\xB9\xB3\xC9\x08\x81\x0B\x10\xA1\xB5\x60"
"\x01\x99\xEB\x9F\x44\xAE\xF4\xFD\xA4\x93\xB8\x1A\x9E\x3D\x84\xF6"
"\x32\x12\x4E\xF0\x23\x6E\x5D\x1E\x3B\x7E\x28\xFA\xE7\xAA\x04\x0A"
"\x2D\x5B\x25\x21\x76\x45\x9D\x1F\x39\x75\x41\xBA\x2A\x58\xFB\x65"
"\x99";

    static unsigned char q[] =
"\x00\xC9\x7F\xB1\xF0\x27\xF4\x53\xF6\x34\x12\x33\xEA\xAA\xD1\xD9"
"\x35\x3F\x6C\x42\xD0\x88\x66\xB1\xD0\x5A\x0F\x20\x35\x02\x8B\x9D"
"\x86\x98\x40\xB4\x16\x66\xB4\x2E\x92\xEA\x0D\xA3\xB4\x32\x04\xB5"
"\xCF\xCE\x33\x52\x52\x4D\x04\x16\xA5\xA4\x41\xE7\x00\xAF\x46\x15"
"\x03";

    static unsigned char dmp1[] =
"\x54\x49\x4C\xA6\x3E\xBA\x03\x37\xE4\xE2\x40\x23\xFC\xD6\x9A\x5A"
"\xEB\x07\xDD\xDC\x01\x83\xA4\xD0\xAC\x9B\x54\xB0\x51\xF2\xB1\x3E"
"\xD9\x49\x09\x75\xEA\xB7\x74\x14\xFF\x59\xC1\xF7\x69\x2E\x9A\x2E"
"\x20\x2B\x38\xFC\x91\x0A\x47\x41\x74\xAD\xC9\x3C\x1F\x67\xC9\x81";

    static unsigned char dmq1[] =
"\x47\x1E\x02\x90\xFF\x0A\xF0\x75\x03\x51\xB7\xF8\x78\x86\x4C\xA9"
"\x61\xAD\xBD\x3A\x8A\x7E\x99\x1C\x5C\x05\x56\xA9\x4C\x31\x46\xA7"
"\xF9\x80\x3F\x8F\x6F\x8A\xE3\x42\xE9\x31\xFD\x8A\xE4\x7A\x22\x0D"
"\x1B\x99\xA4\x95\x84\x98\x07\xFE\x39\xF9\x24\x5A\x98\x36\xDA\x3D";
    
    static unsigned char iqmp[] =
"\x00\xB0\x6C\x4F\xDA\xBB\x63\x01\x19\x8D\x26\x5B\xDB\xAE\x94\x23"
"\xB3\x80\xF2\x71\xF7\x34\x53\x88\x50\x93\x07\x7F\xCD\x39\xE2\x11"
"\x9F\xC9\x86\x32\x15\x4F\x58\x83\xB1\x67\xA9\x67\xBF\x40\x2B\x4E"
"\x9E\x2E\x0F\x96\x56\xE6\x98\xEA\x36\x66\xED\xFB\x25\x79\x80\x39"
"\xF7";

    static unsigned char ctext_ex[] =
"\xb8\x24\x6b\x56\xa6\xed\x58\x81\xae\xb5\x85\xd9\xa2\x5b\x2a\xd7"
"\x90\xc4\x17\xe0\x80\x68\x1b\xf1\xac\x2b\xc3\xde\xb6\x9d\x8b\xce"
"\xf0\xc4\x36\x6f\xec\x40\x0a\xf0\x52\xa7\x2e\x9b\x0e\xff\xb5\xb3"
"\xf2\xf1\x92\xdb\xea\xca\x03\xc1\x27\x40\x05\x71\x13\xbf\x1f\x06"
"\x69\xac\x22\xe9\xf3\xa7\x85\x2e\x3c\x15\xd9\x13\xca\xb0\xb8\x86"
"\x3a\x95\xc9\x92\x94\xce\x86\x74\x21\x49\x54\x61\x03\x46\xf4\xd4"
"\x74\xb2\x6f\x7c\x48\xb4\x2e\xe6\x8e\x1f\x57\x2a\x1f\xc4\x02\x6a"
"\xc4\x56\xb4\xf5\x9f\x7b\x62\x1e\xa1\xb9\xd8\x8f\x64\x20\x2f\xb1";

    SetKey;
    }

static int32	_rtl865x_testCryptCmd(uint32 userId,  int32 argc,int8 **saved){
	int32 size;
	uint32 bits;
	int8 *nextToken;
	
	cle_getNextCmdToken(&nextToken,&size,saved); 
	if(!strcmp(nextToken, "rsa")) {
		RSA *key;
		static unsigned char ptext_ex[] = "\x54\x85\x9b\x34\x2c\x49\xea\x2a";
		int plen, clen = 0, dlen, v;
		
		key = RSA_new();
		plen = sizeof(ptext_ex) - 1;

		for (v = 0; v < 3; v++) {
			key = RSA_new();

			switch (v) {
				case 0:
					clen = key1(key, ctext_ex);
				break;
				case 1:
					clen = key2(key, ctext_ex);
				break;
				case 2:
					clen = key3(key, ctext_ex);
				break;
			}
			clen = RSA_public_encrypt(plen, ptext_ex, ctext, key, RSA_PKCS1_PADDING);
			if(clen < 0) {
				rtlglue_printf("Encryption failed\n");
				return SUCCESS;
			}
			else
				rtlglue_printf("Encryption result data length %d\n", clen);
			dlen = RSA_private_decrypt(clen, ctext, ptext, key, RSA_PKCS1_PADDING);
			if(dlen < 0) {
				rtlglue_printf("Decryption failed\n");
				return SUCCESS;
			}
			else
				rtlglue_printf("Decryption result data length %d\n", dlen);
			if (dlen != plen || memcmp(ptext, ptext_ex, dlen) != 0)
			    rtlglue_printf("PKCS#1 v1.5 encrypted data cannot decrypt to original one!\n");
			else
			    rtlglue_printf("PKCS #1 v1.5 encryption/decryption ok\n");
		}
	} 
	else if(!strcmp(nextToken, "dh")) {
		DH *a;
		DH *b=NULL;
		char buf[12];
		unsigned char *abuf=NULL,*bbuf=NULL;
		int i,alen,blen,aout,bout,ret=1;
		double tm;

		a=DH_generate_parameters(48,DH_GENERATOR_5,cb,NULL);

		if (a == NULL) goto dherr;

		if (!DH_check(a, &i)) goto dherr;
		if (i & DH_CHECK_P_NOT_PRIME)
			rtlglue_printf( "p value is not prime\n");
		if (i & DH_CHECK_P_NOT_SAFE_PRIME)
			rtlglue_printf( "p value is not a safe prime\n");
		if (i & DH_UNABLE_TO_CHECK_GENERATOR)
			rtlglue_printf( "unable to check the generator value\n");
		if (i & DH_NOT_SUITABLE_GENERATOR)
			rtlglue_printf( "the g value is not a generator\n");

		rtlglue_printf("\np    =");
		BN_print(a->p);
		rtlglue_printf("\ng    =");
		BN_print(a->g);
		rtlglue_printf("\n");

		b=DH_new();
		if (b == NULL) goto dherr;

		b->p=BN_dup(a->p);
		b->g=BN_dup(a->g);
		if ((b->p == NULL) || (b->g == NULL)) goto dherr;

		if (!DH_generate_key(a)) goto dherr;
		rtlglue_printf("pri 1=");
		BN_print(a->priv_key);
		rtlglue_printf("\npub 1=");
		BN_print(a->pub_key);
		rtlglue_printf("\n");

		if (!DH_generate_key(b)) goto dherr;
		rtlglue_printf("pri 2=");
		BN_print(b->priv_key);
		rtlglue_printf("\npub 2=");
		BN_print(b->pub_key);
		rtlglue_printf("\n");

		alen=DH_size(a);
		abuf=(unsigned char *)rtlglue_malloc(alen);
		aout=DH_compute_key(abuf,b->pub_key,a);

		rtlglue_printf("key1 =");
		for (i=0; i<aout; i++)
			{
			sprintf(buf,"%02X",abuf[i]);
			rtlglue_printf(buf);
			}
		rtlglue_printf("\n");

		blen=DH_size(b);
		bbuf=(unsigned char *)rtlglue_malloc(blen);
		bout=DH_compute_key(bbuf,a->pub_key,b);

		rtlglue_printf("key2 =");
		for (i=0; i<bout; i++)
			{
			sprintf(buf,"%02X",bbuf[i]);
			rtlglue_printf(buf);
			}
		rtlglue_printf("\n");
		if ((aout < 4) || (bout != aout) || (memcmp(abuf,bbuf,aout) != 0))
			rtlglue_printf("Error in DH routines\n");
dherr:
		if (abuf != NULL) rtlglue_free(abuf);
		if (bbuf != NULL) rtlglue_free(bbuf);
		if(b != NULL) DH_free(b);
		if(a != NULL) DH_free(a);
		return SUCCESS;
	}
	return SUCCESS;
}

cle_exec_t rtl865x_pubkey_cmds[] = {
	{	"performance",
		"Crypto engine performance commands",
		" { rsa %d'bits' | dh %d'bits' } ",
		_rtl865x_perfCryptCmd,
		CLE_USECISCOCMDPARSER,	
		NULL,
		NULL
	},
	{	"test",
		"Crypto engine testing commands",
		" { rsa | dh } ",
		_rtl865x_testCryptCmd,
		CLE_USECISCOCMDPARSER,	
		NULL,
		NULL
	},
};

