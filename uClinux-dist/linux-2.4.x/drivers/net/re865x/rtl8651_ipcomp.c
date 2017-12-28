/*
 * IPCOMP zlib interface code.
 * Copyright (C) 2000  Svenning Soerensen <svenning@post5.tele.dk>
 * Copyright (C) 2000, 2001  Richard Guy Briggs <rgb@conscoop.ottawa.on.ca>
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
 */

char ipcomp_algorithm_c_version[] = "RCSID $Id: rtl8651_ipcomp.c,v 1.1.2.1 2007/09/28 14:42:22 davidm Exp $";

/* SSS */

#include <linux/config.h>
#include <linux/version.h>

#define __NO_VERSION__
#include <linux/module.h>
#include <linux/kernel.h> /* printk() */

//#include "openswan/ipsec_param.h"

#ifdef MALLOC_SLAB
# include <linux/slab.h> /* kmalloc() */
#else /* MALLOC_SLAB */
# include <linux/malloc.h> /* kmalloc() */
#endif /* MALLOC_SLAB */
#include <linux/errno.h>  /* error codes */
#include <linux/types.h>
#include <linux/netdevice.h>
#include <linux/ip.h>
#include <linux/skbuff.h>

#include <linux/netdevice.h>   /* struct device, and other headers */
#include <linux/etherdevice.h> /* eth_type_trans */
#include <linux/ip.h>          /* struct iphdr */
#include <linux/skbuff.h>
#include <asm/uaccess.h>
#include <asm/checksum.h>

//#include <openswan.h>

#include <net/ip.h>

//#include "openswan/radij.h"
//#include "openswan/ipsec_encap.h"
//#include "openswan/ipsec_sa.h"

//#include "openswan/ipsec_xform.h"
//#include "openswan/ipsec_tunnel.h"
//#include "openswan/ipsec_rcv.h" /* sysctl_ipsec_inbound_policy_check */
//#include "openswan/ipsec_proto.h"
//#include "openswan/ipcomp.h"

#include "linux/zlib.h"
#include "linux/zutil.h"

//#include <pfkeyv2.h> /* SADB_X_CALG_DEFLATE */
#include "rtl865x/rtl8651_ipsec_ipcomp.h"
#include "rtl865x/mbuf.h"
#include "rtl865x/assert.h"

#define PKT_BUF_SZ		2048 /* Size of each temporary Rx buffer.*/
#define PKT_HDR_REMAIN    250   /*size remain for pkt hdr*/

#define IPSEC_DEBUG_INFO_SHOW
#undef IPSEC_DEBUG_INFO_SHOW

#ifdef IPSEC_DEBUG_INFO_SHOW
#define IPSEC_DEBUG_INFO(fmt, args...) \
		do {printk("[%s-%d]-info-: " fmt "\n", __FUNCTION__, __LINE__, ## args);} while (0);
#else
#define IPSEC_DEBUG_INFO(fmt, args...) \
		do {} while (0);
#endif

int32 _rtl8651_compress(struct rtl_pktHdr *pkthdr,void *piph,uint32 spi, int16 mtu);
 int32 _rtl8651_deCompress(struct rtl_pktHdr *pkthdr, void *piph, uint32 spi, int16 mtu);

/* ==================================================================
  *
  *					register Compress and Decompress functions into RomeDriver.
  *
  * ================================================================= */
#if defined(CONFIG_RTL865X_IPSEC)&&defined(CONFIG_RTL865X_MULTILAYER_BSP)
int32 rtl8651_registerCompDecompFun(void)
{
#if 1
	printk("%s Not implemented in snapgear kernel yet !!!\n", __FUNCTION__);
	return FAILED;
#else
	int32 retval;
	retval = rtl8651_registerIpcompCallbackFun(_rtl8651_compress, _rtl8651_deCompress);
	if(retval != FAILED)    /*ok,register successfully.record this entry*/
	{
		ipCompIndexSet(retval);
	}
	return retval;
#endif
}
#endif

/* ==================================================================
  *
  *					Compress and Decompress functions implementation.
  *
  * ================================================================= */

static
voidpf ipcomp_zcalloc(voidpf opaque, uInt items, uInt size)
{
	return (voidpf) kmalloc(items*size, GFP_ATOMIC);
}

static
void ipcomp_zfree(voidpf opaque, voidpf address)
{
	kfree(address);
}

int32 _rtl8651_compress(struct rtl_pktHdr *pkthdr,void *piph,uint32 spi, int16 mtu)
{
#if 1
	printk("%s Not implemented in snapgear kernel yet !!!\n", __FUNCTION__);
	return FAILED;
#else
	unsigned int iphlen, pyldsz, cpyldsz;
	unsigned char *buffer;
	struct ip *iph;
	z_stream zs;
	int zresult;

	if(!piph)
	{
		IPSEC_DEBUG_INFO("error: parameter piph == NULL !\n");
		return FAILED;
	}

	iph = (struct ip *)piph;

	switch (iph->ip_p) {
	case IPPROTO_COMP:
	case IPPROTO_AH:
	case IPPROTO_ESP:
		IPSEC_DEBUG_INFO("not compressable because of IPCOMP or AH or ESP packet!\n");
		return FAILED;
	}
	
	/* Don't compress packets already fragmented */
	if (iph->ip_off& __constant_htons(IP_MF | IP_OFFSET)) 
	{
		IPSEC_DEBUG_INFO("not compressable because of fragmented packet!\n");
		return FAILED;
	}
	
	iphlen = ((iph->ip_vhl)&0x0f)<< 2;
	pyldsz = ntohs(iph->ip_len) - iphlen;

	/* Don't compress less than 90 bytes (rfc 2394) */
	if (pyldsz < 90) 
	{
		IPSEC_DEBUG_INFO("not compressable because packet length less than 90 bytes!\n");
		return FAILED;
	}

	zs.zalloc = ipcomp_zcalloc;
	zs.zfree = ipcomp_zfree;
	zs.opaque = 0;
	
	/* We want to use deflateInit2 because we don't want the adler
	   header. */
	zresult = deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -11,
			       			DEF_MEM_LEVEL,  Z_DEFAULT_STRATEGY);
	if (zresult != Z_OK) 
	{
		IPSEC_DEBUG_INFO("not compressable because call deflateInit2() failed!\n");
		return FAILED;
	}

	/* Max output size. Result should be max this size.
	 * Implementation specific tweak:
	 * If it's not at least 32 bytes and 6.25% smaller than
	 * the original packet, it's probably not worth wasting
	 * the receiver's CPU cycles decompressing it.
	 * Your mileage may vary.
	 */
	cpyldsz = pyldsz - sizeof(struct ipcomphdr) - (pyldsz <= 512 ? 32 : pyldsz >> 4);

	buffer =(unsigned char *)kmalloc(cpyldsz, GFP_ATOMIC);
	if (!buffer) 
	{
		IPSEC_DEBUG_INFO("error when allocate buffer!\n");
		deflateEnd(&zs);
		return FAILED;
	}

#ifdef IPSEC_DEBUG_INFO_SHOW
	ipsec_dmp_block("before compress", (char *)iph, ntohs(iph->ip_len));	
#endif /* IPSEC_DEBUG_INFO_SHOW */

	zs.next_in = (char *) iph + iphlen; /* start of payload */
	zs.avail_in = pyldsz;
	zs.next_out = buffer;     /* start of compressed payload */
	zs.avail_out = cpyldsz;
	
	/* Finish compression in one step */
	zresult = deflate(&zs, Z_FINISH);

	/* Free all dynamically allocated buffers */
	deflateEnd(&zs);
	if (zresult != Z_STREAM_END) {
		IPSEC_DEBUG_INFO("deflate error!\n");
		kfree(buffer);
		return FAILED;
	}
	
	/* resulting compressed size */
	cpyldsz -= zs.avail_out;
	
	/* Insert IPCOMP header */
	((struct ipcomphdr*) ((char*) iph + iphlen))->ipcomp_nh = iph->ip_p;
	((struct ipcomphdr*) ((char*) iph + iphlen))->ipcomp_flags = 0;
	/* use the bottom 16 bits of the spi for the cpi.  The top 16 bits are
	   for internal reference only. */
	((struct ipcomphdr*) (((char*)iph) + iphlen))->ipcomp_cpi = htons((__u16)(ntohl(spi) & 0x0000ffff));

	/* Update IP header */
	iph->ip_p = IPPROTO_COMP;
	iph->ip_len = htons(iphlen + sizeof(struct ipcomphdr) + cpyldsz);
#if 1 /* XXX checksum is done by ipsec_tunnel ? */
	iph->ip_sum = 0;
	iph->ip_sum= ip_fast_csum((char *) iph, (iph->ip_vhl)&0x0f);
#endif
	
	/* Copy compressed payload */
	memcpy((char *) iph + iphlen + sizeof(struct ipcomphdr),buffer,cpyldsz);
	kfree(buffer);

	pkthdr->ph_mbuf->m_len = iph->ip_len + 14 /*sizeof(etherHdr_t)*/;
	pkthdr->ph_len =  iph->ip_len + 14;

#ifdef IPSEC_DEBUG_INFO_SHOW
	ipsec_dmp_block("after compress", (char *)iph, ntohs(iph->ip_len));	
#endif /* IPSEC_DEBUG_INFO_SHOW */
	
	return SUCCESS;
#endif
}

 int32 _rtl8651_deCompress(struct rtl_pktHdr *pkthdr, void *piph, uint32 spi, int16 mtu)
{
#if 1
	printk("%s Not implemented in snapgear kernel yet !!!\n", __FUNCTION__);
	return FAILED;
#else
	struct ip *ip;
	unsigned int  pyldsz, cpyldsz;
	z_stream zs;
	int zresult;
	uint8	*buftemp;
	
	if(!piph)
	{
		IPSEC_DEBUG_INFO("error: parameter piph == NULL !\n");
		return FAILED;
	}

	ip = (struct ip *)piph;

	if(ip->ip_p != IPPROTO_COMP)
	{
		IPSEC_DEBUG_INFO("packet is not a ipcomp one!\n");
		return FAILED;
	}

	if (((struct ipcomphdr*)((char*) ip + sizeof(struct ip)))->ipcomp_flags != 0)
	{
		IPSEC_DEBUG_INFO("error: ipcomp_flags != 0 !\n");
		return FAILED;
	}

	if (((struct ipcomphdr*)((char*) ip + sizeof(struct ip)))->ipcomp_cpi != spi)
	{
		IPSEC_DEBUG_INFO(" ipcomp_cpi error !\n");
		return FAILED;
	}
	
	if (ntohs(ip->ip_off) & ~0x4000) 
	{	
		IPSEC_DEBUG_INFO("cannot decompress because of fragmented packet !\n");
		return FAILED;
	}	

	/* original compressed payload size */
	cpyldsz = ntohs(ip->ip_len) - sizeof(struct ip) - sizeof(struct ipcomphdr);

	buftemp =  kmalloc((cpyldsz+4), GFP_ATOMIC);

	if (buftemp == NULL)
	{
		printk("malloc memory error. Runout.\n");
	}

	memcpy(buftemp, (char *)ip+sizeof(struct ip)+sizeof(struct ipcomphdr), cpyldsz);

	zs.zalloc = ipcomp_zcalloc;
	zs.zfree = ipcomp_zfree;
	zs.opaque = 0;
	
	zs.next_in = buftemp;
	zs.avail_in = cpyldsz;

	/* Maybe we should be a bit conservative about memory
	   requirements and use inflateInit2 */
	/* Beware, that this might make us unable to decompress packets
	   from other implementations - HINT: check PGPnet source code */
	/* We want to use inflateInit2 because we don't want the adler
	   header. */
	zresult = inflateInit2(&zs, -15); 
	if (zresult != Z_OK)
	{
		IPSEC_DEBUG_INFO("cannot decompress because call inflateInit2() failed!\n");
		kfree(buftemp);
		return FAILED;
	}
	
	/* We have no way of knowing the exact length of the resulting
	   decompressed output before we have actually done the decompression.
	   For now, we guess that the packet will not be bigger than the
	   attached ipsec device's mtu or 16260, whichever is biggest.
	   This may be wrong, since the sender's mtu may be bigger yet.
	   XXX This must be dealt with later XXX
	*/
	
	/* max payload size */
	pyldsz = mtu < 16260 ? (PKT_BUF_SZ - PKT_HDR_REMAIN) :(65520 - sizeof(struct ip));      /*16260??*/
//	pyldsz = mtu < 16260 ? 1500 :(65520 - sizeof(struct ip));      /*16260??*/

#ifdef IPSEC_DEBUG_INFO_SHOW
	ipsec_dmp_block("before decompress", (char *)ip, ntohs(ip->ip_len));	
#endif /* IPSEC_DEBUG_INFO_SHOW */

	zs.next_out = (char *)ip + sizeof(struct ip);
	zs.avail_out = pyldsz;
	zresult = inflate(&zs, Z_SYNC_FLUSH);

	/* work around a bug in zlib, which sometimes wants to taste an extra
	 * byte when being used in the (undocumented) raw deflate mode.
	 */
	if (zresult == Z_OK && !zs.avail_in && zs.avail_out) {
		__u8 zerostuff = 0;	
		zs.next_in = &zerostuff;
		zs.avail_in = 1;
		zresult = inflate(&zs, Z_FINISH);
	}

	inflateEnd(&zs);
	if (zresult != Z_STREAM_END) {
		IPSEC_DEBUG_INFO("inflate error!\n");
		kfree(buftemp);
		return FAILED;
	}
	
	/* Update IP header */
	/* resulting decompressed size */
	pyldsz -= zs.avail_out;
	ip->ip_len = htons(sizeof(struct ip) + pyldsz);
	ip->ip_p = ((struct ipcomphdr*) ((char*) ip + sizeof(struct ip)))->ipcomp_nh;
	
	if (ip->ip_p == IPPROTO_COMP)
	{
		IPSEC_DEBUG_INFO("error: inner data is also compressed data!\n");
		kfree(buftemp);
		return FAILED;
	}

	pkthdr->ph_mbuf->m_len += pyldsz - cpyldsz - sizeof(struct ipcomphdr);
	pkthdr->ph_len += pyldsz - cpyldsz - sizeof(struct ipcomphdr);
	kfree(buftemp);

#ifdef IPSEC_DEBUG_INFO_SHOW
	ipsec_dmp_block("after decompress", (char *)ip2, ntohs(ip2->ip_len));	
#endif /* IPSEC_DEBUG_INFO_SHOW */

	return SUCCESS;
#endif
}


