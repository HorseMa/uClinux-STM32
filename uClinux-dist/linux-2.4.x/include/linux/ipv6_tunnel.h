/*
 * $Id: s.ipv6_tunnel.h 1.2 02/08/02 12:31:56-00:00 ville $
 */

#ifndef _IPV6_TUNNEL_H
#define _IPV6_TUNNEL_H

#define IPV6_TLV_TNL_ENCAP_LIMIT 4
#define IPV6_DEFAULT_TNL_ENCAP_LIMIT 4

/* don't add encapsulation limit, if one isn't present in inner packet */
#define IPV6_TNL_F_IGN_ENCAP_LIMIT 0x1
/* copy the traffic class field from the inner packet */
#define IPV6_TNL_F_USE_ORIG_TCLASS 0x2
/* also encapsulate packets originating from the tunnel entry-point node */
#define IPV6_TNL_F_ALLOW_LOCAL 0x4
/* created and maintaned from within the kernel */
#define IPV6_TNL_F_KERNEL_DEV 0x8
/* being used for Mobile IPv6 */
#define IPV6_TNL_F_MIPV6_DEV 0x10
/* only capable of receiving packets because local address isn't unicast */
#define IPV6_TNL_F_RCV_ONLY 0x20

struct ipv6_tnl_parm {
	char name[IFNAMSIZ];	/* name of tunnel device */
	int link;		/* ifindex of underlying link layer interface */
	__u8 proto;		/* tunnel protocol */
	__u8 encap_limit;	/* encapsulation limit for tunnel */
	__u8 hop_limit;		/* hop limit for tunnel */
	__u32 flow_lbl;		/* flow  label for tunnel */
	__u32 flags;		/* tunnel flags */
	struct in6_addr laddr;	/* local tunnel end-point address */
	struct in6_addr raddr;	/* remote tunnel end-point address */
};

#endif
