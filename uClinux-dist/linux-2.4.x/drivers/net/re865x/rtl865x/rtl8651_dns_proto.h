/*
*
* Copyright c                  Realtek Semiconductor Corporation, 2005
* All rights reserved.
* 
* Program :	rtl8651_dns.h
* Abstract :	External Header file for DNS related protocol structure in ROMEDRV
* Creator :	Yi-Lun Chen (chenyl@realtek.com.tw)
* Author :	Yi-Lun Chen (chenyl@realtek.com.tw)
*
*/

#ifndef _RTL8651_DNS_PROTO_H
#define _RTL8651_DNS_PROTO_H

/*
		RFC 1035 ( 4.1.1. , p.26) :

		The header contains the following fields:
		                                                                1    1    1    1    1    1
		       0    1    2    3    4    5    6    7    8    9    0    1    2    3    4    5
		    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
		    |                                            ID                                           |
		    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
		    |QR|      Opcode     |AA|TC|RD|RA|       Z      |      RCODE     |
		    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
		    |                                       QDCOUNT                                     |
		    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
		    |                                       ANCOUNT                                      |
		    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
		    |                                       NSCOUNT                                      |
		    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
		    |                                       ARCOUNT                                      |
		    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+

*/
typedef struct _rtl8651_dns_dnsHeader_s {
	uint16				id;
	uint16				qr:1;
	uint16				opcode:4;
	uint16				aa:1;
	uint16				tc:1;
	uint16				rd:1;
	uint16				ra:1;
	uint16				z:3;
	uint16				rcode:4;
	uint16				qdcount;
	uint16				ancount;
	uint16				nscount;
	uint16				arcount;
} _rtl8651_dns_dnsHeader_t;

/* qr */
#define _RTL8651_DNSHDR_QR_QUERY			0	/* query */
#define _RTL8651_DNSHDR_QR_RESPONSE		1	/* response */

/* opcode */
#define _RTL8651_DNSHDR_OPCODE_QUERY	0	/* a standard query */
#define _RTL8651_DNSHDR_OPCODE_IQUERY	1	/* an inverse query */
#define _RTL8651_DNSHDR_OPCODE_STATUS	2	/* a server status request */

/* rcode */
#define _RTL8651_DNSHDR_RCODE_OK			0	/* no error */
#define _RTL8651_DNSHDR_RCODE_FMTERR		1	/* format error */
#define _RTL8651_DNSHDR_RCODE_SVRFAIL	2	/* server failure */
#define _RTL8651_DNSHDR_RCODE_NAMEERR	3	/* name error */
#define _RTL8651_DNSHDR_RCODE_NOIMP		4	/* not implemented */
#define _RTL8651_DNSHDR_RCODE_REFUSED	5	/* refused */

/*
		RFC 1035 ( 3.2.1. , p.11 ) :

		                                                                1    1    1    1    1    1
		       0    1    2    3    4    5    6    7    8    9    0    1    2    3    4    5
		    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
                  |                                                                                           |
                  /                                                                                           /
                  /                                          NAME                                         /
                  |                                                                                           |
		    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
                  |                                          TYPE                                          |
		    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
                  |                                         CLASS                                        |
		    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
                  |                                           TTL                                           |
                  |                                                                                           |
		    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
                  |                                      RDLENGTH                                     |
		    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
                  /                                         RDATA                                        /
                  /                                                                                           /
		    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
*/
typedef struct _rtl8651_dns_dnsRR_fixedField_s {
	uint16	type;
	uint16	class;
	uint32	ttl;
	uint16	rdlength;
} _rtl8651_dns_dnsRR_fixedField_t;

/* type : RFC 1035, 3.2.2 */
#define _RTL8651_DNSTYPE_A				1	/* a host address */
#define _RTL8651_DNSTYPE_NS			2	/* an authoritative name server */
#define _RTL8651_DNSTYPE_MD			3	/* a mail destination (obsolete - use MX) */
#define _RTL8651_DNSTYPE_MF			4	/* a mail forwarder (obsolete - use MX) */
#define _RTL8651_DNSTYPE_CNAME		5	/* the canonical name for an alias */
#define _RTL8651_DNSTYPE_SOA			6	/* marks the start of a zone of authority */
#define _RTL8651_DNSTYPE_MB			7	/* a mailbox domain name (EXPERIMENTAL) */
#define _RTL8651_DNSTYPE_MG			8	/* a mail group member (EXPERIMENTAL) */
#define _RTL8651_DNSTYPE_MR			9	/* a mail rename domain name (EXPERIMENTAL) */
#define _RTL8651_DNSTYPE_NULL			10	/* a null RR (EXPERIMENTAL) */
#define _RTL8651_DNSTYPE_WKS			11	/* a well known service description */
#define _RTL8651_DNSTYPE_PTR			12	/* a domain name pointer */
#define _RTL8651_DNSTYPE_HINFO		13	/* host information */
#define _RTL8651_DNSTYPE_MINFO		14	/* mailbox or mail list information */
#define _RTL8651_DNSTYPE_MX			15	/* mail exchange */
#define _RTL8651_DNSTYPE_TXT			16	/* text strings */

/* qtype: RFC 1035, 3.2.3 */
#define _RTL8651_DNSTYPE_AXFR			252	/* a request for a transfer of an entire zone */
#define _RTL8651_DNSTYPE_MAILB		253	/* a request for mailbox-related records (MB, MG or MR) */
#define _RTL8651_DNSTYPE_MAILA		254	/* a request for mail agent RRs (Obsolete - see MX) */
#define _RTL8651_DNSTYPE_STAR			255	/* a request for all records */

/* class: RFC 1035, 3.2.4 */
#define _RTL8651_DNSCLASS_IN			1	/* the Internet */
#define _RTL8651_DNSCLASS_CS			2	/* the CSNET class (obsolete - used only for examples in some obsolete RFCs) */
#define _RTL8651_DNSCLASS_CH			3	/* the CHAOS class */
#define _RTL8651_DNSCLASS_HS			4	/* Hesiod [S. Dter, F.Hsu, "Hesiod", Project Athena Technical Plan - Name Service, April 1987, version 1.9] */

/* qclass RFC 1035, 3.2.4 */
#define _RTL8651_DNSCLASS_STAR		255	/* any class */

#define _RTL8651_DNS_MAXLABELLEN		63	/* RFC 1035, 4.1.4, Labels are restricted to 63 octets or less. */

#endif /* _RTL8651_DNS_PROTO_H */
