
#ifndef RTL8651_IPSEC_IPCOMP_H
#define RTL8651_IPSEC_IPCOMP_H
#include "rtl_types.h"

#ifndef RTL8651_TBLDRV_PROTO_H
struct ip {

	/* replace bit field */
	uint8 ip_vhl;

	uint8	ip_tos;			/* type of service */
	uint16	ip_len;			/* total length */
	uint16	ip_id;			/* identification */
	uint16	ip_off;			/* fragment offset field */
	uint8	ip_ttl;			/* time to live */
	uint8	ip_p;			/* protocol */
	uint16	ip_sum;			/* checksum */
	struct	in_addr ip_src,ip_dst;	/* source and dest address */
};

#define ETHER_ADDR_LEN	6
struct	ether_header {
	uint8	ether_dhost[ETHER_ADDR_LEN];
	uint8	ether_shost[ETHER_ADDR_LEN];
	uint16	ether_type;
};
#endif

int32 rtl8651_registerIpcompCallbackFun(int32 (*compress)(struct rtl_pktHdr *pkthdr,void *iph,uint32 spi, int16 mtu),
											 int32 (*decompress)(struct rtl_pktHdr *pkthdr,void *iph,uint32 spi, int16 mtu));

#endif

