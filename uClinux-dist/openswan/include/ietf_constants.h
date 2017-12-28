/* manifest constants
 *
 * Copyright (C) 2004       Michael Richardson <mcr@xelerance.com>
 * Copyright (C) 1997 Angelos D. Keromytis.
 * Copyright (C) 1998-2002  D. Hugh Redelmeier.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.  See <http://www.fsf.org/copyleft/gpl.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * RCSID $Id: ietf_constants.h,v 1.18 2005/10/03 19:58:12 mcr Exp $
 */

/* Group parameters from draft-ietf-ike-01.txt section 6 */

#define MODP_GENERATOR "2"

#define MODP768_MODULUS \
    "FFFFFFFF FFFFFFFF C90FDAA2 2168C234 C4C6628B 80DC1CD1 " \
    "29024E08 8A67CC74 020BBEA6 3B139B22 514A0879 8E3404DD " \
    "EF9519B3 CD3A431B 302B0A6D F25F1437 4FE1356D 6D51C245 " \
    "E485B576 625E7EC6 F44C42E9 A63A3620 FFFFFFFF FFFFFFFF"

#define MODP1024_MODULUS \
    "FFFFFFFF FFFFFFFF C90FDAA2 2168C234 C4C6628B 80DC1CD1 " \
    "29024E08 8A67CC74 020BBEA6 3B139B22 514A0879 8E3404DD " \
    "EF9519B3 CD3A431B 302B0A6D F25F1437 4FE1356D 6D51C245 " \
    "E485B576 625E7EC6 F44C42E9 A637ED6B 0BFF5CB6 F406B7ED " \
    "EE386BFB 5A899FA5 AE9F2411 7C4B1FE6 49286651 ECE65381 " \
    "FFFFFFFF FFFFFFFF"

#define MODP1536_MODULUS \
    "FFFFFFFF FFFFFFFF C90FDAA2 2168C234 C4C6628B 80DC1CD1 " \
    "29024E08 8A67CC74 020BBEA6 3B139B22 514A0879 8E3404DD " \
    "EF9519B3 CD3A431B 302B0A6D F25F1437 4FE1356D 6D51C245 " \
    "E485B576 625E7EC6 F44C42E9 A637ED6B 0BFF5CB6 F406B7ED " \
    "EE386BFB 5A899FA5 AE9F2411 7C4B1FE6 49286651 ECE45B3D " \
    "C2007CB8 A163BF05 98DA4836 1C55D39A 69163FA8 FD24CF5F " \
    "83655D23 DCA3AD96 1C62F356 208552BB 9ED52907 7096966D " \
    "670C354E 4ABC9804 F1746C08 CA237327 FFFFFFFF FFFFFFFF "

/* draft-ietf-ipsec-ike-modp-groups-03.txt */
#define MODP2048_MODULUS \
	"FFFFFFFF FFFFFFFF C90FDAA2 2168C234 C4C6628B 80DC1CD1" \
	"29024E08 8A67CC74 020BBEA6 3B139B22 514A0879 8E3404DD" \
	"EF9519B3 CD3A431B 302B0A6D F25F1437 4FE1356D 6D51C245" \
	"E485B576 625E7EC6 F44C42E9 A637ED6B 0BFF5CB6 F406B7ED" \
	"EE386BFB 5A899FA5 AE9F2411 7C4B1FE6 49286651 ECE45B3D" \
	"C2007CB8 A163BF05 98DA4836 1C55D39A 69163FA8 FD24CF5F" \
	"83655D23 DCA3AD96 1C62F356 208552BB 9ED52907 7096966D" \
	"670C354E 4ABC9804 F1746C08 CA18217C 32905E46 2E36CE3B" \
	"E39E772C 180E8603 9B2783A2 EC07A28F B5C55DF0 6F4C52C9" \
	"DE2BCBF6 95581718 3995497C EA956AE5 15D22618 98FA0510" \
	"15728E5A 8AACAA68 FFFFFFFF FFFFFFFF"

#define MODP3072_MODULUS \
	"FFFFFFFF FFFFFFFF C90FDAA2 2168C234 C4C6628B 80DC1CD1" \
	"29024E08 8A67CC74 020BBEA6 3B139B22 514A0879 8E3404DD" \
	"EF9519B3 CD3A431B 302B0A6D F25F1437 4FE1356D 6D51C245" \
	"E485B576 625E7EC6 F44C42E9 A637ED6B 0BFF5CB6 F406B7ED" \
	"EE386BFB 5A899FA5 AE9F2411 7C4B1FE6 49286651 ECE45B3D" \
	"C2007CB8 A163BF05 98DA4836 1C55D39A 69163FA8 FD24CF5F" \
	"83655D23 DCA3AD96 1C62F356 208552BB 9ED52907 7096966D" \
	"670C354E 4ABC9804 F1746C08 CA18217C 32905E46 2E36CE3B" \
	"E39E772C 180E8603 9B2783A2 EC07A28F B5C55DF0 6F4C52C9" \
	"DE2BCBF6 95581718 3995497C EA956AE5 15D22618 98FA0510" \
	"15728E5A 8AAAC42D AD33170D 04507A33 A85521AB DF1CBA64" \
	"ECFB8504 58DBEF0A 8AEA7157 5D060C7D B3970F85 A6E1E4C7" \
	"ABF5AE8C DB0933D7 1E8C94E0 4A25619D CEE3D226 1AD2EE6B" \
	"F12FFA06 D98A0864 D8760273 3EC86A64 521F2B18 177B200C" \
	"BBE11757 7A615D6C 770988C0 BAD946E2 08E24FA0 74E5AB31" \
	"43DB5BFC E0FD108E 4B82D120 A93AD2CA FFFFFFFF FFFFFFFF"

#define MODP4096_MODULUS \
	"FFFFFFFF FFFFFFFF C90FDAA2 2168C234 C4C6628B 80DC1CD1" \
	"29024E08 8A67CC74 020BBEA6 3B139B22 514A0879 8E3404DD" \
	"EF9519B3 CD3A431B 302B0A6D F25F1437 4FE1356D 6D51C245" \
	"E485B576 625E7EC6 F44C42E9 A637ED6B 0BFF5CB6 F406B7ED" \
	"EE386BFB 5A899FA5 AE9F2411 7C4B1FE6 49286651 ECE45B3D" \
	"C2007CB8 A163BF05 98DA4836 1C55D39A 69163FA8 FD24CF5F" \
	"83655D23 DCA3AD96 1C62F356 208552BB 9ED52907 7096966D" \
	"670C354E 4ABC9804 F1746C08 CA18217C 32905E46 2E36CE3B" \
	"E39E772C 180E8603 9B2783A2 EC07A28F B5C55DF0 6F4C52C9" \
	"DE2BCBF6 95581718 3995497C EA956AE5 15D22618 98FA0510" \
	"15728E5A 8AAAC42D AD33170D 04507A33 A85521AB DF1CBA64" \
	"ECFB8504 58DBEF0A 8AEA7157 5D060C7D B3970F85 A6E1E4C7" \
	"ABF5AE8C DB0933D7 1E8C94E0 4A25619D CEE3D226 1AD2EE6B" \
	"F12FFA06 D98A0864 D8760273 3EC86A64 521F2B18 177B200C" \
	"BBE11757 7A615D6C 770988C0 BAD946E2 08E24FA0 74E5AB31" \
	"43DB5BFC E0FD108E 4B82D120 A9210801 1A723C12 A787E6D7" \
	"88719A10 BDBA5B26 99C32718 6AF4E23C 1A946834 B6150BDA" \
	"2583E9CA 2AD44CE8 DBBBC2DB 04DE8EF9 2E8EFC14 1FBECAA6" \
	"287C5947 4E6BC05D 99B2964F A090C3A2 233BA186 515BE7ED" \
	"1F612970 CEE2D7AF B81BDD76 2170481C D0069127 D5B05AA9" \
	"93B4EA98 8D8FDDC1 86FFB7DC 90A6C08F 4DF435C9 34063199" \
	"FFFFFFFF FFFFFFFF"

/* copy&pasted from rfc3526: */
#define MODP6144_MODULUS \
	"FFFFFFFF FFFFFFFF C90FDAA2 2168C234 C4C6628B 80DC1CD1 29024E08" \
	"8A67CC74 020BBEA6 3B139B22 514A0879 8E3404DD EF9519B3 CD3A431B" \
	"302B0A6D F25F1437 4FE1356D 6D51C245 E485B576 625E7EC6 F44C42E9" \
	"A637ED6B 0BFF5CB6 F406B7ED EE386BFB 5A899FA5 AE9F2411 7C4B1FE6" \
	"49286651 ECE45B3D C2007CB8 A163BF05 98DA4836 1C55D39A 69163FA8" \
	"FD24CF5F 83655D23 DCA3AD96 1C62F356 208552BB 9ED52907 7096966D" \
	"670C354E 4ABC9804 F1746C08 CA18217C 32905E46 2E36CE3B E39E772C" \
	"180E8603 9B2783A2 EC07A28F B5C55DF0 6F4C52C9 DE2BCBF6 95581718" \
	"3995497C EA956AE5 15D22618 98FA0510 15728E5A 8AAAC42D AD33170D" \
	"04507A33 A85521AB DF1CBA64 ECFB8504 58DBEF0A 8AEA7157 5D060C7D" \
	"B3970F85 A6E1E4C7 ABF5AE8C DB0933D7 1E8C94E0 4A25619D CEE3D226" \
	"1AD2EE6B F12FFA06 D98A0864 D8760273 3EC86A64 521F2B18 177B200C" \
	"BBE11757 7A615D6C 770988C0 BAD946E2 08E24FA0 74E5AB31 43DB5BFC" \
	"E0FD108E 4B82D120 A9210801 1A723C12 A787E6D7 88719A10 BDBA5B26" \
	"99C32718 6AF4E23C 1A946834 B6150BDA 2583E9CA 2AD44CE8 DBBBC2DB" \
	"04DE8EF9 2E8EFC14 1FBECAA6 287C5947 4E6BC05D 99B2964F A090C3A2" \
	"233BA186 515BE7ED 1F612970 CEE2D7AF B81BDD76 2170481C D0069127" \
	"D5B05AA9 93B4EA98 8D8FDDC1 86FFB7DC 90A6C08F 4DF435C9 34028492" \
	"36C3FAB4 D27C7026 C1D4DCB2 602646DE C9751E76 3DBA37BD F8FF9406" \
	"AD9E530E E5DB382F 413001AE B06A53ED 9027D831 179727B0 865A8918" \
	"DA3EDBEB CF9B14ED 44CE6CBA CED4BB1B DB7F1447 E6CC254B 33205151" \
	"2BD7AF42 6FB8F401 378CD2BF 5983CA01 C64B92EC F032EA15 D1721D03" \
	"F482D7CE 6E74FEF6 D55E702F 46980C82 B5A84031 900B1C9E 59E7C97F" \
	"BEC7E8F3 23A97A7E 36CC88BE 0F1D45B7 FF585AC5 4BD407B2 2B4154AA" \
	"CC8F6D7E BF48E1D8 14CC5ED2 0F8037E0 A79715EE F29BE328 06A1D58B" \
	"B7C5DA76 F550AA3D 8A1FBFF0 EB19CCB1 A313D55C DA56C9EC 2EF29632" \
	"387FE8D7 6E3C0468 043E8F66 3F4860EE 12BF2D5B 0B7474D6 E694F91E" \
	"6DCC4024 FFFFFFFF FFFFFFFF"

/* copy&pasted from rfc3526: */
#define MODP8192_MODULUS \
	"FFFFFFFF FFFFFFFF C90FDAA2 2168C234 C4C6628B 80DC1CD1" \
	"29024E08 8A67CC74 020BBEA6 3B139B22 514A0879 8E3404DD" \
	"EF9519B3 CD3A431B 302B0A6D F25F1437 4FE1356D 6D51C245" \
	"E485B576 625E7EC6 F44C42E9 A637ED6B 0BFF5CB6 F406B7ED" \
	"EE386BFB 5A899FA5 AE9F2411 7C4B1FE6 49286651 ECE45B3D" \
	"C2007CB8 A163BF05 98DA4836 1C55D39A 69163FA8 FD24CF5F" \
	"83655D23 DCA3AD96 1C62F356 208552BB 9ED52907 7096966D" \
	"670C354E 4ABC9804 F1746C08 CA18217C 32905E46 2E36CE3B" \
	"E39E772C 180E8603 9B2783A2 EC07A28F B5C55DF0 6F4C52C9" \
	"DE2BCBF6 95581718 3995497C EA956AE5 15D22618 98FA0510" \
	"15728E5A 8AAAC42D AD33170D 04507A33 A85521AB DF1CBA64" \
	"ECFB8504 58DBEF0A 8AEA7157 5D060C7D B3970F85 A6E1E4C7" \
	"ABF5AE8C DB0933D7 1E8C94E0 4A25619D CEE3D226 1AD2EE6B" \
	"F12FFA06 D98A0864 D8760273 3EC86A64 521F2B18 177B200C" \
	"BBE11757 7A615D6C 770988C0 BAD946E2 08E24FA0 74E5AB31" \
	"43DB5BFC E0FD108E 4B82D120 A9210801 1A723C12 A787E6D7" \
	"88719A10 BDBA5B26 99C32718 6AF4E23C 1A946834 B6150BDA" \
	"2583E9CA 2AD44CE8 DBBBC2DB 04DE8EF9 2E8EFC14 1FBECAA6" \
	"287C5947 4E6BC05D 99B2964F A090C3A2 233BA186 515BE7ED" \
	"1F612970 CEE2D7AF B81BDD76 2170481C D0069127 D5B05AA9" \
	"93B4EA98 8D8FDDC1 86FFB7DC 90A6C08F 4DF435C9 34028492" \
	"36C3FAB4 D27C7026 C1D4DCB2 602646DE C9751E76 3DBA37BD" \
	"F8FF9406 AD9E530E E5DB382F 413001AE B06A53ED 9027D831" \
	"179727B0 865A8918 DA3EDBEB CF9B14ED 44CE6CBA CED4BB1B" \
	"DB7F1447 E6CC254B 33205151 2BD7AF42 6FB8F401 378CD2BF" \
	"5983CA01 C64B92EC F032EA15 D1721D03 F482D7CE 6E74FEF6" \
	"D55E702F 46980C82 B5A84031 900B1C9E 59E7C97F BEC7E8F3" \
	"23A97A7E 36CC88BE 0F1D45B7 FF585AC5 4BD407B2 2B4154AA" \
	"CC8F6D7E BF48E1D8 14CC5ED2 0F8037E0 A79715EE F29BE328" \
	"06A1D58B B7C5DA76 F550AA3D 8A1FBFF0 EB19CCB1 A313D55C" \
	"DA56C9EC 2EF29632 387FE8D7 6E3C0468 043E8F66 3F4860EE" \
	"12BF2D5B 0B7474D6 E694F91E 6DBE1159 74A3926F 12FEE5E4" \
	"38777CB6 A932DF8C D8BEC4D0 73B931BA 3BC832B6 8D9DD300" \
	"741FA7BF 8AFC47ED 2576F693 6BA42466 3AAB639C 5AE4F568" \
	"3423B474 2BF1C978 238F16CB E39D652D E3FDB8BE FC848AD9" \
	"22222E04 A4037C07 13EB57A8 1A23F0C7 3473FC64 6CEA306B" \
	"4BCBC886 2F8385DD FA9D4B7F A2C087E8 79683303 ED5BDD3A" \
	"062B3CF5 B3A278A6 6D2A13F8 3F44F82D DF310EE0 74AB6A36" \
	"4597E899 A0255DC1 64F31CC5 0846851D F9AB4819 5DED7EA1" \
	"B1D510BD 7EE74D73 FAF36BC3 1ECFA268 359046F4 EB879F92" \
	"4009438B 481C6CD7 889A002E D5EE382B C9190DA6 FC026E47" \
	"9558E447 5677E9AA 9E3050E2 765694DF C81F56E8 80B96E71" \
	"60C980DD 98EDD3DF FFFFFFFF FFFFFFFF"

#define LOCALSECRETSIZE		BYTES_FOR_BITS(256)

/* limits on nonce sizes.  See RFC2409 "The internet key exchange (IKE)" 5 */
#define MINIMUM_NONCE_SIZE	8	/* bytes */
#define DEFAULT_NONCE_SIZE	16	/* bytes */
#define MAXIMUM_NONCE_SIZE	256	/* bytes */

#define COOKIE_SIZE 8
#define MAX_ISAKMP_SPI_SIZE 16

#define MD2_DIGEST_SIZE         BYTES_FOR_BITS(128)     /* ought to be supplied by md2.h */
#define MD5_DIGEST_SIZE		BYTES_FOR_BITS(128)	/* ought to be supplied by md5.h */
#define SHA1_DIGEST_SIZE	BYTES_FOR_BITS(160)	/* ought to be supplied by sha1.h */

#define SHA2_256_DIGEST_SIZE	BYTES_FOR_BITS(256)	/* sha2.h */
#define SHA2_384_DIGEST_SIZE	BYTES_FOR_BITS(384)
#define SHA2_512_DIGEST_SIZE	BYTES_FOR_BITS(512)

#define DES_CBC_BLOCK_SIZE	BYTES_FOR_BITS(64)
#define AES_CBC_BLOCK_SIZE      BYTES_FOR_BITS(128)

#define DSS_QBITS	160	/* bits in DSS's "q" (FIPS 186-1) */

/* to statically allocate IV, we need max of
 * MD5_DIGEST_SIZE, SHA1_DIGEST_SIZE, and DES_CBC_BLOCK_SIZE.
 * To avoid combinatorial explosion, we leave out DES_CBC_BLOCK_SIZE.
 */
#define MAX_DIGEST_LEN_OLD (MD5_DIGEST_SIZE > SHA1_DIGEST_SIZE? MD5_DIGEST_SIZE : SHA1_DIGEST_SIZE)
  
/* for max: SHA2_512 */
#define MAX_DIGEST_LEN (512/BITS_PER_BYTE)

/* RFC 2404 "HMAC-SHA-1-96" section 3 */
#define HMAC_SHA1_KEY_LEN    SHA1_DIGEST_SIZE

/* RFC 2403 "HMAC-MD5-96" section 3 */
#define HMAC_MD5_KEY_LEN    MD5_DIGEST_SIZE

#define IKE_UDP_PORT	500

/* Version numbers - IKEv1 */
#define ISAKMP_MAJOR_VERSION   0x1
#define ISAKMP_MINOR_VERSION   0x0

/* version numbers - IKEv2 */
#define IKEv2_MAJOR_VERSION    0x2
#define IKEv2_MINOR_VERSION    0x0


/* Domain of Interpretation */
#define ISAKMP_DOI_ISAKMP          0
#define ISAKMP_DOI_IPSEC           1

/* IPsec DOI things */

#define IPSEC_DOI_SITUATION_LENGTH 4
#define IPSEC_DOI_LDI_LENGTH       4
#define IPSEC_DOI_SPI_SIZE         4

/* SPI value 0 is invalid and values 1-255 are reserved to IANA.
 * ESP: RFC 2402 2.4; AH: RFC 2406 2.1
 * IPComp RFC 2393 substitutes a CPI in the place of an SPI.
 * see also draft-shacham-ippcp-rfc2393bis-05.txt.
 * We (Openswan) reserve 0x100 to 0xFFF for manual keying, so
 * Pluto won't generate these values.
 */
#define IPSEC_DOI_SPI_MIN          0x100
#define IPSEC_DOI_SPI_OUR_MIN      0x1000

/* Payload types
 * RFC2408 Internet Security Association and Key Management Protocol (ISAKMP)
 * section 3.1
 *
 * RESERVED 14-127
 * Private USE 128-255
 */

enum next_payload_types {
	ISAKMP_NEXT_NONE     =  0,	/* No other payload following */
	ISAKMP_NEXT_SA       =  1,	/* Security Association */
	ISAKMP_NEXT_P        =  2,	/* Proposal */
	ISAKMP_NEXT_T        =  3,	/* Transform */
	ISAKMP_NEXT_KE       =  4,	/* Key Exchange */
	ISAKMP_NEXT_ID       =  5,	/* Identification */
	ISAKMP_NEXT_CERT     =  6,	/* Certificate */
	ISAKMP_NEXT_CR       =  7,	/* Certificate Request */
	ISAKMP_NEXT_HASH     =  8,	/* Hash */
	ISAKMP_NEXT_SIG      =  9,	/* Signature */
	ISAKMP_NEXT_NONCE    =  10,	/* Nonce */
	ISAKMP_NEXT_N        =  11,	/* Notification */
	ISAKMP_NEXT_D        =  12,	/* Delete */
	ISAKMP_NEXT_VID      =  13,	/* Vendor ID */
	ISAKMP_NEXT_ATTR     =  14,       /* Mode config Attribute */
	ISAKMP_NEXT_NATD_BADDRAFTS =15, /* NAT-Traversal: NAT-D (bad drafts) */
                                /* !!! Conflicts with RFC 3547 */
	ISAKMP_NEXT_NATD_RFC  = 20,       /* NAT-Traversal: NAT-D (rfc) */
	ISAKMP_NEXT_NATOA_RFC = 21,       /* NAT-Traversal: NAT-OA (rfc) */

	ISAKMP_NEXT_v2SA  = 33,          /* security association */
	ISAKMP_NEXT_v2KE  = 34,          /* key exchange payload */
	ISAKMP_NEXT_v2IDi = 35,          /* Initiator ID payload */
	ISAKMP_NEXT_v2IDr = 36,          /* Responder ID payload */
	ISAKMP_NEXT_v2CERT= 37,          /* Certificate */
	ISAKMP_NEXT_v2CERTREQ= 38,       /* Certificate Request */
	ISAKMP_NEXT_v2AUTH= 39,          /* Authentication */
	ISAKMP_NEXT_v2Ni  = 40,          /* Nonce - initiator */
	ISAKMP_NEXT_v2Nr  = 40,          /* Nonce - responder */
	ISAKMP_NEXT_v2N   = 41,          /* Notify */
	ISAKMP_NEXT_v2D   = 42,          /* Delete */
	ISAKMP_NEXT_v2V   = 43,          /* Vendor ID */
	ISAKMP_NEXT_v2TSi = 44,          /* Traffic Selector, initiator */
	ISAKMP_NEXT_v2TSr = 45,          /* Traffic Selector, responder */
	ISAKMP_NEXT_v2E   = 46,          /* Encrypted payload */
	ISAKMP_NEXT_v2CP  = 47,          /* Configuration payload (MODECFG) */
	ISAKMP_NEXT_v2EAP = 48,          /* Extensible authentication*/

	ISAKMP_NEXT_ROOF  = 49,	         /* roof on payload types */

	/* SPECIAL CASES */
	ISAKMP_NEXT_NATD_DRAFTS  = 130,   /* NAT-Traversal: NAT-D (drafts) */
	ISAKMP_NEXT_NATOA_DRAFTS = 131   /* NAT-Traversal: NAT-OA (drafts) */
};

/* These values are to be used within the Type field of an Attribute (14) 
 *    ISAKMP payload.  */
#define ISAKMP_CFG_REQUEST         1
#define ISAKMP_CFG_REPLY           2
#define ISAKMP_CFG_SET             3
#define ISAKMP_CFG_ACK             4

/* Mode Config attribute values */
#define    INTERNAL_IP4_ADDRESS        1
#define    INTERNAL_IP4_NETMASK        2
#define    INTERNAL_IP4_DNS            3
#define    INTERNAL_IP4_NBNS           4
#define    INTERNAL_ADDRESS_EXPIRY     5
#define    INTERNAL_IP4_DHCP           6
#define    APPLICATION_VERSION         7
#define    INTERNAL_IP6_ADDRESS        8
#define    INTERNAL_IP6_NETMASK        9
#define    INTERNAL_IP6_DNS           10
#define    INTERNAL_IP6_NBNS          11
#define    INTERNAL_IP6_DHCP          12
#define    INTERNAL_IP4_SUBNET        13
#define    SUPPORTED_ATTRIBUTES       14
#define    INTERNAL_IP6_SUBNET        15

/* XAUTH attribute values */
#define    XAUTH_TYPE                16520
#define    XAUTH_USER_NAME           16521
#define    XAUTH_USER_PASSWORD       16522
#define    XAUTH_PASSCODE            16523
#define    XAUTH_MESSAGE             16524
#define    XAUTH_CHALLENGE           16525
#define    XAUTH_DOMAIN              16526
#define    XAUTH_STATUS              16527
#define    XAUTH_NEXT_PIN            16528
#define    XAUTH_ANSWER              16529

#define XAUTH_TYPE_GENERIC 0
#define XAUTH_TYPE_CHAP    1
#define XAUTH_TYPE_OTP     2
#define XAUTH_TYPE_SKEY    3

/*
  extern enum_names modecfg_attr_names;
  extern enum_names xauth_type_names;
*/

/* Exchange types
 * RFC2408 "Internet Security Association and Key Management Protocol (ISAKMP)"
 * section 3.1
 *
 * ISAKMP Future Use     6 - 31
 * DOI Specific Use     32 - 239
 * Private Use         240 - 255
 *
 * Note: draft-ietf-ipsec-dhless-enc-mode-00.txt Appendix A
 * defines "DHless RSA Encryption" as 6.
 */

/*
  extern enum_names exchange_names;
*/

enum isakmp_xchg_types {
	ISAKMP_XCHG_NONE=0,
	ISAKMP_XCHG_BASE=1,
	ISAKMP_XCHG_IDPROT=2,	/* ID Protection */
	ISAKMP_XCHG_AO=3,	/* Authentication Only */
	ISAKMP_XCHG_AGGR=4,	/* Aggressive */
	ISAKMP_XCHG_INFO=5,	/* Informational */
	ISAKMP_XCHG_MODE_CFG=6,      /* Mode Config */

        /* Private exchanges to pluto -- tried to write an RFC */
	ISAKMP_XCHG_ECHOREQUEST=30,    /* Echo Request */
	ISAKMP_XCHG_ECHOREPLY=31,      /* Echo Reply   */

        /* Extra exchange types, defined by Oakley
         * RFC2409 "The Internet Key Exchange (IKE)", near end of Appendix A
         */
	ISAKMP_XCHG_QUICK=32,	/* Oakley Quick Mode */
	ISAKMP_XCHG_NGRP=33,	/* Oakley New Group Mode */

        /* IKEv2 things */
	ISAKMP_v2_SA_INIT=34,
	ISAKMP_v2_AUTH=35,
	ISAKMP_v2_CHILD_SA=36,
	ISAKMP_v2_INFORMATIONAL=37,

	ISAKMP_XCHG_ECHOREQUEST_PRIVATE=244,     /* Private Echo Request */
	ISAKMP_XCHG_ECHOREPLY_PRIVATE=245,     /* Private Echo Reply   */
};

/* Flag bits */
#define ISAKMP_FLAGS_E         (1<<0)     /* bit 0 of flags --- encrypt  */
#define ISAKMP_FLAGS_C         (1<<1)     /* bit 1 of flags --- commit   */
#define ISAKMP_FLAGS_I         (1<<3)     /* bit 3 of flags --- initiator */
#define ISAKMP_FLAGS_V         (1<<4)     /* bit 4 of flags --- version */
#define ISAKMP_FLAGS_R         (1<<5)     /* bit 5 of flags --- response */
extern const char *const flag_bit_names[];

#define ISAKMP_FLAG_ENCRYPTION   0x1  /* repeat of above */
#define ISAKMP_FLAG_COMMIT       0x2


/* Situation definition for IPsec DOI */
extern const char *const sit_bit_names[];

#define SIT_IDENTITY_ONLY        0x01
#define SIT_SECRECY              0x02
#define SIT_INTEGRITY            0x04


/* Critical bit in each payload */
/* extern enum_names critical_names; */
#define ISAKMP_PAYLOAD_NONCRITICAL  0x00
#define ISAKMP_PAYLOAD_CRITICAL     0x80

/* Protocol IDs
 * RFC2407 The Internet IP security Domain of Interpretation for ISAKMP 4.4.1
 */

/*
 * extern enum_names protocol_names;
 * same in IKEv1 and IKEv2.
 */
#define PROTO_RESERVED           0  /* only in IKEv2 */
#define PROTO_ISAKMP             1
#define PROTO_IPSEC_AH           2
#define PROTO_IPSEC_ESP          3
#define PROTO_IPCOMP             4  /* only in IKEv1 */

/*
 * IKEv2 proposal
 * See http://www.iana.org/assignments/ikev2-parameters 
 */
enum ikev2_trans_type {
	IKEv2_TRANS_TYPE_ENCR = 1,
	IKEv2_TRANS_TYPE_PRF  = 2,
	IKEv2_TRANS_TYPE_INTEG= 3,
	IKEv2_TRANS_TYPE_DH   = 4,   /* same as in IKEv1 */
	IKEv2_TRANS_TYPE_ESN  = 5,
};

enum ikev2_trans_type_encr {
	IKEv2_ENCR_DES_IV64 = 1,
	IKEv2_ENCR_DES      = 2,
	IKEv2_ENCR_3DES     = 3,
	IKEv2_ENCR_RC5      = 4,
	IKEv2_ENCR_IDEA     = 5,
	IKEv2_ENCR_CAST     = 6,
	IKEv2_ENCR_BLOWFISH = 7,
	IKEv2_ENCR_3IDEA    = 8,
	IKEv2_ENCR_DES_IV32 = 9,
	IKEv2_ENCR_RES10    = 10,
	IKEv2_ENCR_NULL     = 11,
	IKEv2_ENCR_AES_CBC  = 12,
	IKEv2_ENCR_AES_CTR  = 13,
	IKEv2_ENCR_AES_CCM_8  = 14,
	IKEv2_ENCR_AES_CCM_12 = 15,
	IKEv2_ENCR_AES_CCM_16 = 16,
	IKEv2_UNASSIGNED_17   = 17,
	IKEv2_ENCR_AES_GCM_8  = 18,
	IKEv2_ENCR_AES_GCM_12 = 19,
	IKEv2_ENCR_AES_GCM_16 = 20,
	IKEv2_ENC_NULL_AUTH_AES_GMAC = 21,
	IKEv2_RESERVED_IEEE_P1619_XTS_AES = 22,
	/* 23 - 1023 Reserved to IANA */
	/* 1024 - 65535 Private Use */
	IKEv2_ENCR_INVALID  = 65536
};

enum ikev2_trans_type_prf {
	IKEv2_PRF_HMAC_MD5      = 1, /* RFC2104 */
	IKEv2_PRF_HMAC_SHA1     = 2, /* RFC2104 */
	IKEv2_PRF_HMAC_TIGER    = 3, /* RFC2104 */
	IKEv2_PRF_AES128_XCBC   = 4, /* RFC4434 */
	IKEv2_PRF_HMAC_SHA2_256 = 5, /* RFC4868 */
	IKEv2_PRF_HMAC_SHA2_384 = 6, /* RFC4868 */
	IKEv2_PRF_HMAC_SHA2_512 = 7, /* RFC4868 */
	IKEv2_PRF_AES128_CMAC   = 8, /* RFC4615 */
	/* 9 - 1023 Reserved to IANA    RFC4306 */
	/* 1024 - 65535 Private Use     RFC4306 */
};

enum ikev2_trans_type_integ {
	IKEv2_AUTH_NONE              = 0,  /* RFC4306 */
	IKEv2_AUTH_HMAC_MD5_96       = 1,  /* RFC2403 */
	IKEv2_AUTH_HMAC_SHA1_96      = 2,  /* RFC2404 */
	IKEv2_AUTH_DES_MAC           = 3,  /* RFC4306 */
	IKEv2_AUTH_KPDK_MD5          = 4,  /* RFC1826 */
	IKEv2_AUTH_AES_XCBC_96       = 5,  /* RFC3566 */
	IKEv2_AUTH_HMAC_MD5_128      = 6,  /* RFC4595 */
	IKEv2_AUTH_HMAC_SHA1_160     = 7,  /* RFC4595 */
	IKEv2_AUTH_AES_CMAC_96       = 8,  /* RFC4494 */
	IKEv2_AUTH_AES_128_GMAC      = 9,  /* RFC4543 */
	IKEv2_AUTH_AES_192_GMAC      = 10, /* RFC4543 */
	IKEv2_AUTH_AES_256_GMAC      = 11, /* RFC4543 */
	IKEv2_AUTH_HMAC_SHA2_256_128 = 12, /* RFC4595 */
	IKEv2_AUTH_HMAC_SHA2_384_192 = 13, /* RFC4306 */
	IKEv2_AUTH_HMAC_SHA2_512_256 = 14, /* RFC4306 */
	/* 15 - 1023 Reserved to IANA         RFC4306 */
	/* 1024 - 65535 Private Use           RFC4306 */
	IKEv2_AUTH_INVALID     =65536
};

enum ikev2_trans_type_esn {
	IKEv2_ESN_DISABLED = 0,
	IKEv2_ESN_ENABLED  = 1,
};

/* RFC 4306 Section 3.3.5 */	
enum ikev2_trans_attr_type {
	IKEv2_KEY_LENGTH = 14,
};


/* many transform values are moved to openswan/ipsec_policy.h
 * including all of the following, which are here so that
 * they will get caught by grep:
 */

enum ipsec_policy_command;
struct ipsec_policy_msg_head;
enum ipsec_privacy_quality;
enum ipsec_bandwidth_quality;
enum ipsec_authentication_algo;
enum ipsec_cipher_algo;
enum ipsec_comp_algo;
enum ipsec_id_type;
enum ipsec_cert_type;
struct ipsec_dns_sig;
struct ipsec_raw_key;
struct ipsec_identity;
struct ipsec_policy_cmd_query;

/*
  extern enum_names isakmp_transformid_names;
*/

#define KEY_IKE               1

/*
  extern enum_names ah_transformid_names;
  extern enum_names esp_transformid_names;
  extern enum_names ipcomp_transformid_names;
*/

/* the following are from RFC 2393/draft-shacham-ippcp-rfc2393bis-05.txt 3.3 */
typedef u_int16_t cpi_t;
#define IPCOMP_CPI_SIZE          2
#define IPCOMP_FIRST_NEGOTIATED  256
#define IPCOMP_LAST_NEGOTIATED   61439

/* Identification type values
 * RFC 2407 The Internet IP security Domain of Interpretation for ISAKMP 4.6.2.1
 */

/*
  extern enum_names ident_names;
*/

/* actual enum for ipsec_cert_type, e.g. CERT_NONE is in openswan/ipsec_policy.h */
/*
  extern enum_names cert_type_names;
*/
  

/* Oakley transform attributes
 * draft-ietf-ipsec-ike-01.txt appendix A
 */

/*
  extern enum_names oakley_attr_names;
  extern const char *const oakley_attr_bit_names[];
*/

enum ikev1_oakley_attr {
	OAKLEY_ENCRYPTION_ALGORITHM   =1,
	OAKLEY_HASH_ALGORITHM         =2,
	OAKLEY_AUTHENTICATION_METHOD  =3,
	OAKLEY_GROUP_DESCRIPTION      =4,
	OAKLEY_GROUP_TYPE             =5,
	OAKLEY_GROUP_PRIME            =6,	/* B/V */
	OAKLEY_GROUP_GENERATOR_ONE    =7,	/* B/V */
	OAKLEY_GROUP_GENERATOR_TWO    =8,	/* B/V */
	OAKLEY_GROUP_CURVE_A          =9,	/* B/V */
	OAKLEY_GROUP_CURVE_B         =10,	/* B/V */
	OAKLEY_LIFE_TYPE             =11,
	OAKLEY_LIFE_DURATION         =12,	/* B/V */
	OAKLEY_PRF                   =13,
	OAKLEY_KEY_LENGTH            =14,
	OAKLEY_FIELD_SIZE            =15,
	OAKLEY_GROUP_ORDER           =16,	/* B/V */
	OAKLEY_BLOCK_SIZE            =17,
};

/* for each Oakley attribute, which enum_names describes its values? */
/* extern enum_names *oakley_attr_val_descs[]; */

/* IPsec DOI attributes
 * RFC2407 The Internet IP security Domain of Interpretation for ISAKMP 4.5
 */

/* extern enum_names ipsec_attr_names; */

enum ikev1_ipsec_attr {
	SA_LIFE_TYPE            =1,
	SA_LIFE_DURATION        =2,	/* B/V */
	GROUP_DESCRIPTION       =3,
	ENCAPSULATION_MODE      =4,
	AUTH_ALGORITHM          =5,
	KEY_LENGTH              =6,
	KEY_ROUNDS              =7,
	COMPRESS_DICT_SIZE      =8,
	COMPRESS_PRIVATE_ALG    =9,	/* B/V */
};

/* for each IPsec attribute, which enum_names describes its values? */
/* extern enum_names *ipsec_attr_val_descs[]; */

/* SA Lifetime Type attribute
 * RFC2407 The Internet IP security Domain of Interpretation for ISAKMP 4.5
 * Default time specified in 4.5
 *
 * There are two defaults for IPSEC SA lifetime, SA_LIFE_DURATION_DEFAULT,
 * and PLUTO_SA_LIFE_DURATION_DEFAULT.
 * SA_LIFE_DURATION_DEFAULT is specified in RFC2407 "The Internet IP
 * Security Domain of Interpretation for ISAKMP" 4.5.  It applies when
 * an ISAKMP negotiation does not explicitly specify a life duration.
 * PLUTO_SA_LIFE_DURATION_DEFAULT is specified in pluto(8).  It applies
 * when a connection description does not specify --ipseclifetime.
 * The value of SA_LIFE_DURATION_MAXIMUM is our local policy.
 */

/* extern enum_names sa_lifetime_names; */

#define SA_LIFE_TYPE_SECONDS   1
#define SA_LIFE_TYPE_KBYTES    2

#define SA_LIFE_DURATION_DEFAULT    28800 /* eight hours (RFC2407 4.5) */
#define PLUTO_SA_LIFE_DURATION_DEFAULT    28800 /* eight hours (pluto(8)) */
#define SA_LIFE_DURATION_MAXIMUM    86400 /* one day */

#define SA_REPLACEMENT_MARGIN_DEFAULT	    540	  /* (IPSEC & IKE) nine minutes */
#define SA_REPLACEMENT_FUZZ_DEFAULT	    100	  /* (IPSEC & IKE) 100% of MARGIN */
#define SA_REPLACEMENT_RETRIES_DEFAULT	    0	/*  (IPSEC & IKE) */

#define SA_LIFE_DURATION_K_DEFAULT  0xFFFFFFFFlu

/* Encapsulation Mode attribute */

/* extern enum_names enc_mode_names; */

#define ENCAPSULATION_MODE_UNSPECIFIED 0	/* not legal -- used internally */
#define ENCAPSULATION_MODE_TUNNEL      1
#define ENCAPSULATION_MODE_TRANSPORT   2

#define ENCAPSULATION_MODE_UDP_TUNNEL_DRAFTS       61443
#define ENCAPSULATION_MODE_UDP_TRANSPORT_DRAFTS    61444
#define ENCAPSULATION_MODE_UDP_TUNNEL_RFC          3
#define ENCAPSULATION_MODE_UDP_TRANSPORT_RFC       4

#ifdef NAT_TRAVERSAL
#define ENCAPSULATION_MODE_UDP_TUNNEL_DRAFTS       61443
#define ENCAPSULATION_MODE_UDP_TRANSPORT_DRAFTS    61444
#define ENCAPSULATION_MODE_UDP_TUNNEL_RFC          3
#define ENCAPSULATION_MODE_UDP_TRANSPORT_RFC       4
#endif

/* Auth Algorithm attribute */

/* extern enum_names auth_alg_names, extended_auth_alg_names; */

enum ikev1_auth_attribute {
	AUTH_ALGORITHM_NONE=0,	/* our private designation */
	AUTH_ALGORITHM_HMAC_MD5   =1,
	AUTH_ALGORITHM_HMAC_SHA1  =2,
	AUTH_ALGORITHM_DES_MAC    =3,
	AUTH_ALGORITHM_KPDK       =4,
	AUTH_ALGORITHM_HMAC_SHA2_256=5,
	AUTH_ALGORITHM_HMAC_SHA2_384=6,
	AUTH_ALGORITHM_HMAC_SHA2_512=7,
	AUTH_ALGORITHM_HMAC_RIPEMD=8,
	AUTH_ALGORITHM_AES_CBC=9,
};

typedef u_int16_t ipsec_auth_t;

/* Oakley Lifetime Type attribute
 * draft-ietf-ipsec-ike-01.txt appendix A
 * As far as I can see, there is not specification for
 * OAKLEY_ISAKMP_SA_LIFETIME_DEFAULT.  This could lead to interop problems!
 * For no particular reason, we chose one hour.
 * The value of OAKLEY_ISAKMP_SA_LIFETIME_MAXIMUM is our local policy.
 */
/* extern enum_names oakley_lifetime_names; */

#define OAKLEY_LIFE_SECONDS   1
#define OAKLEY_LIFE_KILOBYTES 2

#define OAKLEY_ISAKMP_SA_LIFETIME_DEFAULT 3600    /* one hour */
#define OAKLEY_ISAKMP_SA_LIFETIME_MAXIMUM 86400   /* 1 day */

/* Oakley PRF attribute (none defined)
 * draft-ietf-ipsec-ike-01.txt appendix A
 */
/* extern enum_names oakley_prf_names; */

/* HMAC (see rfc2104.txt) */

#define HMAC_IPAD            0x36
#define HMAC_OPAD            0x5C
#define HMAC_BUFSIZE         64

/* Oakley Encryption Algorithm attribute
 * draft-ietf-ipsec-ike-01.txt appendix A
 * and from http://www.isi.edu/in-notes/iana/assignments/ipsec-registry
 */

/* extern enum_names oakley_enc_names; (IKEv1 only) */

#define OAKLEY_DES_CBC          1
#define OAKLEY_IDEA_CBC         2
#define OAKLEY_BLOWFISH_CBC     3
#define OAKLEY_RC5_R16_B64_CBC  4
#define OAKLEY_3DES_CBC         5
#define OAKLEY_CAST_CBC         6
#define OAKLEY_AES_CBC          7
#define OAKLEY_CAMELLIA_CBC	8
#define OAKLEY_SERPENT_CBC              65004
#define OAKLEY_TWOFISH_CBC              65005
#define OAKLEY_TWOFISH_CBC_SSH          65289

#define OAKLEY_ENCRYPT_MAX      65535	/* pretty useless :) */

/* Oakley Hash Algorithm attribute
 * draft-ietf-ipsec-ike-01.txt appendix A
 * and from http://www.isi.edu/in-notes/iana/assignments/ipsec-registry
 */

typedef u_int16_t oakley_hash_t;
/* extern enum_names oakley_hash_names; */

#define OAKLEY_MD5      1
#define OAKLEY_SHA1     2
#define OAKLEY_SHA      OAKLEY_SHA1
#define OAKLEY_TIGER    3
#define OAKLEY_SHA2_256        4
#define OAKLEY_SHA2_384        5
#define OAKLEY_SHA2_512        6

#define OAKLEY_HASH_MAX      7

/* Oakley Authentication Method attribute
 * draft-ietf-ipsec-ike-01.txt appendix A
 * Goofy Hybrid extensions from draft-ietf-ipsec-isakmp-hybrid-auth-05.txt
 * Goofy XAUTH extensions from draft-ietf-ipsec-isakmp-xauth-06.txt
 */

/* extern enum_names oakley_auth_names; */

#define OAKLEY_PRESHARED_KEY       1
#define OAKLEY_DSS_SIG             2
#define OAKLEY_RSA_SIG             3
#define OAKLEY_RSA_ENC             4
#define OAKLEY_RSA_ENC_REV         5
#define OAKLEY_ELGAMAL_ENC         6
#define OAKLEY_ELGAMAL_ENC_REV     7

#define OAKLEY_AUTH_ROOF           8  /*roof on auth values THAT WE SUPPORT */

#define HybridInitRSA                                     64221
#define HybridRespRSA                                     64222
#define HybridInitDSS                                     64223
#define HybridRespDSS                                     64224

/* For XAUTH, store in st->xauth, and set equivalent in st->auth */
#define XAUTHInitPreShared                                65001
#define XAUTHRespPreShared                                65002
#define XAUTHInitDSS                                      65003
#define XAUTHRespDSS                                      65004
#define XAUTHInitRSA                                      65005
#define XAUTHRespRSA                                      65006
#define XAUTHInitRSAEncryption                            65007
#define XAUTHRespRSAEncryption                            65008
#define XAUTHInitRSARevisedEncryption                     65009
#define XAUTHRespRSARevisedEncryption                     65010

/* typedef to make our life easier */
typedef u_int16_t oakley_auth_t;

/* extern enum_names ikev2_auth_names; */
enum ikev2_auth_method {
	v2_AUTH_RSA = 1,
	v2_AUTH_SHARED=2,
	v2_AUTH_DSA = 3,
};

/* Oakley Group Description attribute
 * draft-ietf-ipsec-ike-01.txt appendix A
 */
/* extern enum_names oakley_group_names; */

typedef enum ike_trans_type_dh oakley_group_t;

/*	you must also touch: constants.c, crypto.c */
enum ike_trans_type_dh {
	OAKLEY_GROUP_MODP768      = 1,
	OAKLEY_GROUP_MODP1024     = 2,
	OAKLEY_GROUP_GP155        = 3,
	OAKLEY_GROUP_GP185        = 4,
	OAKLEY_GROUP_MODP1536     = 5,

	OAKLEY_GROUP_MODP2048     = 14,
	OAKLEY_GROUP_MODP3072     = 15,
	OAKLEY_GROUP_MODP4096     = 16,
	OAKLEY_GROUP_MODP6144     = 17,
	OAKLEY_GROUP_MODP8192     = 18,
};

/* Oakley Group Type attribute
 * draft-ietf-ipsec-ike-01.txt appendix A
 */
/* extern enum_names oakley_group_type_names; */

#define OAKLEY_GROUP_TYPE_MODP     1
#define OAKLEY_GROUP_TYPE_ECP      2
#define OAKLEY_GROUP_TYPE_EC2N     3


/* Notify messages -- error types
 * See RFC2408 ISAKMP 3.14.1
 */

/* extern enum_names notification_names;
   extern enum_names ipsec_notification_names;
*/

typedef enum {
    NOTHING_WRONG =             0,  /* unofficial! */

    INVALID_PAYLOAD_TYPE =       1,
    DOI_NOT_SUPPORTED =          2,
    SITUATION_NOT_SUPPORTED =    3,
    INVALID_COOKIE =             4,
    INVALID_MAJOR_VERSION =      5,
    INVALID_MINOR_VERSION =      6,
    INVALID_EXCHANGE_TYPE =      7,
    INVALID_FLAGS =              8,
    INVALID_MESSAGE_ID =         9,
    INVALID_PROTOCOL_ID =       10,
    INVALID_SPI =               11,
    INVALID_TRANSFORM_ID =      12,
    ATTRIBUTES_NOT_SUPPORTED =  13,
    NO_PROPOSAL_CHOSEN =        14,
    BAD_PROPOSAL_SYNTAX =       15,
    PAYLOAD_MALFORMED =         16,
    INVALID_KEY_INFORMATION =   17,
    INVALID_ID_INFORMATION =    18,
    INVALID_CERT_ENCODING =     19,
    INVALID_CERTIFICATE =       20,
    CERT_TYPE_UNSUPPORTED =     21,
    INVALID_CERT_AUTHORITY =    22,
    INVALID_HASH_INFORMATION =  23,
    AUTHENTICATION_FAILED =     24,
    INVALID_SIGNATURE =         25,
    ADDRESS_NOTIFICATION =      26,
    NOTIFY_SA_LIFETIME =        27,
    CERTIFICATE_UNAVAILABLE =   28,
    UNSUPPORTED_EXCHANGE_TYPE = 29,
    UNEQUAL_PAYLOAD_LENGTHS =   30,

    /* ISAKMP status type */
    CONNECTED =              16384,

    /* IPSEC DOI additions; status types (RFC2407 IPSEC DOI 4.6.3)
     * These must be sent under the protection of an ISAKMP SA.
     */
    IPSEC_RESPONDER_LIFETIME = 24576,
    IPSEC_REPLAY_STATUS =      24577,
    IPSEC_INITIAL_CONTACT =    24578,

    /* RFC 3706 DPD */ 
    R_U_THERE =       36136,
    R_U_THERE_ACK =   36137,

    /* IKEv2 */
    UNSUPPORTED_CRITICAL_PAYLOAD = 1,
    INVALID_IKE_SPI              = 4,
    /*INVALID_MAJOR_VERSION      = 5, */ /* same as ikev1 */
    INVALID_SYNTAX               = 7,
    /*INVALID_MESSAGE_ID         = 9, */ /* same as ikev1 */
    /*INVALID_SPI                =11, */ /* same as ikev1 */
    /*NO_PROPOSAL_CHOSEN         =14, */ /* same as ikev1 */
    INVALID_KE_PAYLOAD           =17,
    /*AUTHENTICATION_FAILED      =24, */ /* same as ikev1 */
    SINGLE_PAIR_REQUIRED         =34,
    NO_ADDITIONAL_SAS            =35,
    INTERNAL_ADDRESS_FAILURE     =36,
    FAILED_CP_REQUIRED           =37,
    TS_UNACCEPTABLE              =38,
    INVALID_SELECTORS            =39,

    INITIAL_CONTACT              =16384,
    SET_WINDOW_SIZE              =16385,
    ADDITIONAL_TS_POSSIBLE       =16386,
    IPCOMP_SUPPORTED             =16387,
    NAT_DETECTION_SOURCE_IP      =16388,
    NAT_DETECTION_DESTINATION_IP =16389,
    COOKIE                       =16390,
    USE_TRANSPORT_MODE           =16391,
    HTTP_CERT_LOOKUP_SUPPORTED   =16392,
    REKEY_SA                     =16393,
    ESP_TFC_PADDING_NOT_SUPPORTED=16394,
    NON_FIRST_FRAGMENTS_ALSO     =16395,

    } notification_t;


/* Public key algorithm number
 * Same numbering as used in DNSsec
 * See RFC 2535 DNSsec 3.2 The KEY Algorithm Number Specification.
 * Also found in BIND 8.2.2 include/isc/dst.h as DST algorithm codes.
 */

enum pubkey_alg
{
    PUBKEY_ALG_RSA = 1,
    PUBKEY_ALG_DSA = 3,
};

/* Limits on size of RSA moduli.
 * The upper bound matches that of DNSsec (see RFC 2537).
 * The lower bound must be more than 11 octets for certain
 * the encoding to work, but it must be much larger for any
 * real security.  For now, we require 512 bits.
 */

#define RSA_MIN_OCTETS_RFC	12

#define RSA_MIN_OCTETS	BYTES_FOR_BITS(512)
#define RSA_MIN_OCTETS_UGH	"RSA modulus too small for security: less than 512 bits"

#define RSA_MAX_OCTETS	BYTES_FOR_BITS(8192)
#define RSA_MAX_OCTETS_UGH	"RSA modulus too large: more than 8192 bits"

/* Note: RFC 2537 encoding adds a few bytes.  If you use a small
 * modulus like 3, the overhead is only 2 bytes
 */
#define RSA_MAX_ENCODING_BYTES	(RSA_MAX_OCTETS + 2)

#define ISA_MAJ_SHIFT	4
#define ISA_MIN_MASK	(~((~0u) << ISA_MAJ_SHIFT))

#define ISAKMP_ATTR_AF_MASK 0x8000
#define ISAKMP_ATTR_AF_TV ISAKMP_ATTR_AF_MASK /* value in lv */
#define ISAKMP_ATTR_AF_TLV 0 /* length in lv; value follows */

#define ISAKMP_ATTR_RTYPE_MASK 0x7FFF

/* NOTE:
 * ID_IPV4_ADDR, ID_FQDN, etc. are defined in openswan/ipsec_policy.h
 * AND: enum_names ident_names is in constants.c
 */

