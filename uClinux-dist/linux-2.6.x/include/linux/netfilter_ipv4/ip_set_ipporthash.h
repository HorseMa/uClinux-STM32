#ifndef __IP_SET_IPPORTHASH_H
#define __IP_SET_IPPORTHASH_H

#include <linux/netfilter_ipv4/ip_set.h>

#define SETTYPE_NAME "ipporthash"
#define MAX_RANGE 0x0000FFFF
#define INVALID_PORT	(MAX_RANGE + 1)

struct ip_set_ipporthash {
	ip_set_ip_t *members;		/* the ipporthash proper */
	uint32_t elements;		/* number of elements */
	uint32_t hashsize;		/* hash size */
	uint16_t probes;		/* max number of probes  */
	uint16_t resize;		/* resize factor in percent */
	ip_set_ip_t first_ip;		/* host byte order, included in range */
	ip_set_ip_t last_ip;		/* host byte order, included in range */
	void *initval[0];		/* initvals for jhash_1word */
};

struct ip_set_req_ipporthash_create {
	uint32_t hashsize;
	uint16_t probes;
	uint16_t resize;
	ip_set_ip_t from;
	ip_set_ip_t to;
};

struct ip_set_req_ipporthash {
	ip_set_ip_t ip;
	ip_set_ip_t port;
};

#endif	/* __IP_SET_IPPORTHASH_H */
