/* Copyright (C) 2003-2004 Jozsef Kadlecsik <kadlec@blackhole.kfki.hu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.  
 */

/* Kernel module implementing an ip hash set */

#include <linux/module.h>
#include <linux/ip.h>
#include <linux/skbuff.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter_ipv4/ip_set.h>
#include <linux/errno.h>
#include <asm/uaccess.h>
#include <asm/bitops.h>
#include <linux/spinlock.h>
#include <linux/vmalloc.h>
#include <linux/random.h>

#include <net/ip.h>

#include <linux/netfilter_ipv4/ip_set_malloc.h>
#include <linux/netfilter_ipv4/ip_set_iphash.h>
//#include <linux/netfilter_ipv4/ip_set_jhash.h>

static int limit = MAX_RANGE;

static inline __u32
jhash_ip(const struct ip_set_iphash *map, uint16_t i, ip_set_ip_t ip)
{
	return jhash_1word(ip, *(((uint32_t *) map->initval) + i));
}

static inline __u32
hash_id(struct ip_set *set, ip_set_ip_t ip, ip_set_ip_t *hash_ip)
{
	struct ip_set_iphash *map = (struct ip_set_iphash *) set->data;
	__u32 id;
	u_int16_t i;
	ip_set_ip_t *elem;

	*hash_ip = ip & map->netmask;
	DP("set: %s, ip:%u.%u.%u.%u, %u.%u.%u.%u, %u.%u.%u.%u",
	   set->name, HIPQUAD(ip), HIPQUAD(*hash_ip), HIPQUAD(map->netmask));
	
	for (i = 0; i < map->probes; i++) {
		id = jhash_ip(map, i, *hash_ip) % map->hashsize;
		DP("hash key: %u", id);
		elem = HARRAY_ELEM(map->members, ip_set_ip_t *, id);
		if (*elem == *hash_ip)
			return id;
		/* No shortcut at testing - there can be deleted
		 * entries. */
	}
	return UINT_MAX;
}

static inline int
__testip(struct ip_set *set, ip_set_ip_t ip, ip_set_ip_t *hash_ip)
{
	return (ip && hash_id(set, ip, hash_ip) != UINT_MAX);
}

static int
testip(struct ip_set *set, const void *data, size_t size,
       ip_set_ip_t *hash_ip)
{
	struct ip_set_req_iphash *req = 
	    (struct ip_set_req_iphash *) data;

	if (size != sizeof(struct ip_set_req_iphash)) {
		ip_set_printk("data length wrong (want %zu, have %zu)",
			      sizeof(struct ip_set_req_iphash),
			      size);
		return -EINVAL;
	}
	return __testip(set, req->ip, hash_ip);
}

static int
testip_kernel(struct ip_set *set, 
	      const struct sk_buff *skb,
	      ip_set_ip_t *hash_ip,
	      const u_int32_t *flags,
	      unsigned char index)
{
	return __testip(set,
			ntohl(flags[index] & IPSET_SRC 
				? ip_hdr(skb)->saddr 
				: ip_hdr(skb)->daddr),
			hash_ip);
}

static inline int
__addip(struct ip_set_iphash *map, ip_set_ip_t ip, ip_set_ip_t *hash_ip)
{
	__u32 probe;
	u_int16_t i;
	ip_set_ip_t *elem;
	
	if (!ip || map->elements > limit)
		return -ERANGE;

	*hash_ip = ip & map->netmask;
	
	for (i = 0; i < map->probes; i++) {
		probe = jhash_ip(map, i, *hash_ip) % map->hashsize;
		elem = HARRAY_ELEM(map->members, ip_set_ip_t *, probe);
		if (*elem == *hash_ip)
			return -EEXIST;
		if (!*elem) {
			*elem = *hash_ip;
			map->elements++;
			return 0;
		}
	}
	/* Trigger rehashing */
	return -EAGAIN;
}

static int
addip(struct ip_set *set, const void *data, size_t size,
        ip_set_ip_t *hash_ip)
{
	struct ip_set_req_iphash *req = 
	    (struct ip_set_req_iphash *) data;

	if (size != sizeof(struct ip_set_req_iphash)) {
		ip_set_printk("data length wrong (want %zu, have %zu)",
			      sizeof(struct ip_set_req_iphash),
			      size);
		return -EINVAL;
	}
	return __addip((struct ip_set_iphash *) set->data, req->ip, hash_ip);
}

static int
addip_kernel(struct ip_set *set, 
	     const struct sk_buff *skb,
	     ip_set_ip_t *hash_ip,
	     const u_int32_t *flags,
	     unsigned char index)
{
	return __addip((struct ip_set_iphash *) set->data,
		       ntohl(flags[index] & IPSET_SRC 
		       		? ip_hdr(skb)->saddr 
				: ip_hdr(skb)->daddr),
		       hash_ip);
}

static int retry(struct ip_set *set)
{
	struct ip_set_iphash *map = (struct ip_set_iphash *) set->data;
	ip_set_ip_t hash_ip, *elem;
	void *members;
	u_int32_t i, hashsize = map->hashsize;
	int res;
	struct ip_set_iphash *tmp;
	
	if (map->resize == 0)
		return -ERANGE;

    again:
    	res = 0;
    	
	/* Calculate new hash size */
	hashsize += (hashsize * map->resize)/100;
	if (hashsize == map->hashsize)
		hashsize++;
	
	ip_set_printk("rehashing of set %s triggered: "
		      "hashsize grows from %u to %u",
		      set->name, map->hashsize, hashsize);

	tmp = kmalloc(sizeof(struct ip_set_iphash) 
		      + map->probes * sizeof(uint32_t), GFP_ATOMIC);
	if (!tmp) {
		DP("out of memory for %d bytes",
		   sizeof(struct ip_set_iphash)
		   + map->probes * sizeof(uint32_t));
		return -ENOMEM;
	}
	tmp->members = harray_malloc(hashsize, sizeof(ip_set_ip_t), GFP_ATOMIC);
	if (!tmp->members) {
		DP("out of memory for %d bytes", hashsize * sizeof(ip_set_ip_t));
		kfree(tmp);
		return -ENOMEM;
	}
	tmp->hashsize = hashsize;
	tmp->elements = 0;
	tmp->probes = map->probes;
	tmp->resize = map->resize;
	tmp->netmask = map->netmask;
	memcpy(tmp->initval, map->initval, map->probes * sizeof(uint32_t));
	
	write_lock_bh(&set->lock);
	map = (struct ip_set_iphash *) set->data; /* Play safe */
	for (i = 0; i < map->hashsize && res == 0; i++) {
		elem = HARRAY_ELEM(map->members, ip_set_ip_t *, i);	
		if (*elem)
			res = __addip(tmp, *elem, &hash_ip);
	}
	if (res) {
		/* Failure, try again */
		write_unlock_bh(&set->lock);
		harray_free(tmp->members);
		kfree(tmp);
		goto again;
	}
	
	/* Success at resizing! */
	members = map->members;

	map->hashsize = tmp->hashsize;
	map->members = tmp->members;
	write_unlock_bh(&set->lock);

	harray_free(members);
	kfree(tmp);

	return 0;
}

static inline int
__delip(struct ip_set *set, ip_set_ip_t ip, ip_set_ip_t *hash_ip)
{
	struct ip_set_iphash *map = (struct ip_set_iphash *) set->data;
	ip_set_ip_t id, *elem;

	if (!ip)
		return -ERANGE;

	id = hash_id(set, ip, hash_ip);
	if (id == UINT_MAX)
		return -EEXIST;
		
	elem = HARRAY_ELEM(map->members, ip_set_ip_t *, id);
	*elem = 0;
	map->elements--;

	return 0;
}

static int
delip(struct ip_set *set, const void *data, size_t size,
        ip_set_ip_t *hash_ip)
{
	struct ip_set_req_iphash *req =
	    (struct ip_set_req_iphash *) data;

	if (size != sizeof(struct ip_set_req_iphash)) {
		ip_set_printk("data length wrong (want %zu, have %zu)",
			      sizeof(struct ip_set_req_iphash),
			      size);
		return -EINVAL;
	}
	return __delip(set, req->ip, hash_ip);
}

static int
delip_kernel(struct ip_set *set, 
	     const struct sk_buff *skb,
	     ip_set_ip_t *hash_ip,
	     const u_int32_t *flags,
	     unsigned char index)
{
	return __delip(set,
		       ntohl(flags[index] & IPSET_SRC 
				? ip_hdr(skb)->saddr 
				: ip_hdr(skb)->daddr),
		       hash_ip);
}

static int create(struct ip_set *set, const void *data, size_t size)
{
	struct ip_set_req_iphash_create *req =
	    (struct ip_set_req_iphash_create *) data;
	struct ip_set_iphash *map;
	uint16_t i;

	if (size != sizeof(struct ip_set_req_iphash_create)) {
		ip_set_printk("data length wrong (want %zu, have %zu)",
			       sizeof(struct ip_set_req_iphash_create),
			       size);
		return -EINVAL;
	}

	if (req->hashsize < 1) {
		ip_set_printk("hashsize too small");
		return -ENOEXEC;
	}

	if (req->probes < 1) {
		ip_set_printk("probes too small");
		return -ENOEXEC;
	}

	map = kmalloc(sizeof(struct ip_set_iphash) 
		      + req->probes * sizeof(uint32_t), GFP_KERNEL);
	if (!map) {
		DP("out of memory for %d bytes",
		   sizeof(struct ip_set_iphash)
		   + req->probes * sizeof(uint32_t));
		return -ENOMEM;
	}
	for (i = 0; i < req->probes; i++)
		get_random_bytes(((uint32_t *) map->initval)+i, 4);
	map->elements = 0;
	map->hashsize = req->hashsize;
	map->probes = req->probes;
	map->resize = req->resize;
	map->netmask = req->netmask;
	map->members = harray_malloc(map->hashsize, sizeof(ip_set_ip_t), GFP_KERNEL);
	if (!map->members) {
		DP("out of memory for %d bytes", map->hashsize * sizeof(ip_set_ip_t));
		kfree(map);
		return -ENOMEM;
	}

	set->data = map;
	return 0;
}

static void destroy(struct ip_set *set)
{
	struct ip_set_iphash *map = (struct ip_set_iphash *) set->data;

	harray_free(map->members);
	kfree(map);

	set->data = NULL;
}

static void flush(struct ip_set *set)
{
	struct ip_set_iphash *map = (struct ip_set_iphash *) set->data;
	harray_flush(map->members, map->hashsize, sizeof(ip_set_ip_t));
	map->elements = 0;
}

static void list_header(const struct ip_set *set, void *data)
{
	struct ip_set_iphash *map = (struct ip_set_iphash *) set->data;
	struct ip_set_req_iphash_create *header =
	    (struct ip_set_req_iphash_create *) data;

	header->hashsize = map->hashsize;
	header->probes = map->probes;
	header->resize = map->resize;
	header->netmask = map->netmask;
}

static int list_members_size(const struct ip_set *set)
{
	struct ip_set_iphash *map = (struct ip_set_iphash *) set->data;

	return (map->hashsize * sizeof(ip_set_ip_t));
}

static void list_members(const struct ip_set *set, void *data)
{
	struct ip_set_iphash *map = (struct ip_set_iphash *) set->data;
	ip_set_ip_t i, *elem;

	for (i = 0; i < map->hashsize; i++) {
		elem = HARRAY_ELEM(map->members, ip_set_ip_t *, i);	
		((ip_set_ip_t *)data)[i] = *elem;
	}
}

static struct ip_set_type ip_set_iphash = {
	.typename		= SETTYPE_NAME,
	.features		= IPSET_TYPE_IP | IPSET_DATA_SINGLE,
	.protocol_version	= IP_SET_PROTOCOL_VERSION,
	.create			= &create,
	.destroy		= &destroy,
	.flush			= &flush,
	.reqsize		= sizeof(struct ip_set_req_iphash),
	.addip			= &addip,
	.addip_kernel		= &addip_kernel,
	.retry			= &retry,
	.delip			= &delip,
	.delip_kernel		= &delip_kernel,
	.testip			= &testip,
	.testip_kernel		= &testip_kernel,
	.header_size		= sizeof(struct ip_set_req_iphash_create),
	.list_header		= &list_header,
	.list_members_size	= &list_members_size,
	.list_members		= &list_members,
	.me			= THIS_MODULE,
};

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jozsef Kadlecsik <kadlec@blackhole.kfki.hu>");
MODULE_DESCRIPTION("iphash type of IP sets");
module_param(limit, int, 0600);
MODULE_PARM_DESC(limit, "maximal number of elements stored in the sets");

static int __init init(void)
{
	return ip_set_register_set_type(&ip_set_iphash);
}

static void __exit fini(void)
{
	/* FIXME: possible race with ip_set_create() */
	ip_set_unregister_set_type(&ip_set_iphash);
}

module_init(init);
module_exit(fini);
