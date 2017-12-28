/* information about connections between hosts and clients
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
 * RCSID $Id: connections.c,v 1.256.2.12 2008-02-18 22:29:11 paul Exp $
 */

#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <resolv.h>
#include <arpa/nameser.h>     /* missing from <resolv.h> on old systems */
#include <sys/queue.h>


#include <openswan.h>
#include <openswan/ipsec_policy.h>
#include "pfkeyv2.h"
#include "kameipsec.h"

#include "constants.h"
#include "defs.h"
#include "id.h"
#include "x509.h"
#include "pgp.h"
#include "certs.h"
#include "ac.h"
#include "smartcard.h"
#ifdef XAUTH_USEPAM
#include <security/pam_appl.h>
#endif
#include "connections.h"	/* needs id.h */
#include "pending.h"
#include "foodgroups.h"
#include "packet.h"
#include "demux.h"	/* needs packet.h */
#include "state.h"
#include "timer.h"
#include "ipsec_doi.h"	/* needs demux.h and state.h */
#include "server.h"
#include "kernel.h"	/* needs connections.h */
#include "log.h"
#include "keys.h"
#include "secrets.h"
#include "adns.h"	/* needs <resolv.h> */
#include "dnskey.h"	/* needs keys.h and adns.h */
#include "whack.h"
#include "alg_info.h"
#include "spdb.h"
#include "ike_alg.h"
#include "kernel_alg.h"
#include "plutoalg.h"
#include "xauth.h"
#ifdef NAT_TRAVERSAL
#include "nat_traversal.h"
#endif

#ifdef VIRTUAL_IP
#include "virtual.h"
#endif

#ifndef ns_t_txt
#define ns_t_txt T_TXT
#endif

#ifndef ns_t_key
#define ns_t_key T_KEY
#endif

static struct connection *connections = NULL;

/* struct host_pair: a nexus of information about a pair of hosts.
 * A host is an IP address, UDP port pair.  This is a debatable choice:
 * - should port be considered (no choice of port in standard)?
 * - should ID be considered (hard because not always known)?
 * - should IP address matter on our end (we don't know our end)?
 * Only oriented connections are registered.
 * Unoriented connections are kept on the unoriented_connections
 * linked list (using hp_next).  For them, host_pair is NULL.
 */

struct host_pair {
    struct {
	ip_address addr;
	u_int16_t  host_port;	        /* IKE port */
	bool       host_port_specific;	/* if above is interesting */
    } me, him;
    bool initial_connection_sent;
    struct connection *connections;	/* connections with this pair */
    struct pending *pending;	/* awaiting Keying Channel */
    struct host_pair *next;
};

static struct host_pair *host_pairs = NULL;

static struct connection *unoriented_connections = NULL;

void host_pair_enqueue_pending(const struct connection *c
			       , struct pending *p
			       , struct pending **pnext)
{
    *pnext = c->host_pair->pending;
    c->host_pair->pending = p;
}

struct pending **host_pair_first_pending(const struct connection *c)
{
    if(c->host_pair == NULL) return NULL;

    return &c->host_pair->pending;
}

    
/* check to see that Ids of peers match */
bool
same_peer_ids(const struct connection *c, const struct connection *d
, const struct id *his_id)
{
    return same_id(&c->spd.this.id, &d->spd.this.id)
	&& same_id(his_id == NULL? &c->spd.that.id : his_id, &d->spd.that.id);
}

/** returns a host pair based upon addresses.
 * 
 * find_host_pair is given a pair of addresses, plus UDP ports, and
 * returns a host_pair entry that covers it. It also moves the relevant
 * pair description to the beginning of the list, so that it can be
 * found faster next time.
 * 
 */
static struct host_pair *
find_host_pair(const ip_address *myaddr
	       , u_int16_t myport
	       , const ip_address *hisaddr
	       , u_int16_t hisport)
{
    struct host_pair *p, *prev;
    char b1[ADDRTOT_BUF],b2[ADDRTOT_BUF];

    /* default hisaddr to an appropriate any */
    if (hisaddr == NULL)
	hisaddr = aftoinfo(addrtypeof(myaddr))->any;

    /*
     * look for a host-pair that has the right set of ports/address.
     *
     */
    
    /*
     * for the purposes of comparison, port 500 and 4500 are identical,
     * but other ports are not.
     * So if any port==4500, then set it to 500.
     */
    if(myport == 4500) myport=500;
    if(hisport== 4500) hisport=500;

    for (prev = NULL, p = host_pairs; p != NULL; prev = p, p = p->next)
    {
	DBG(DBG_CONTROLMORE
	    , DBG_log("find_host_pair: comparing to %s:%d %s:%d\n"
		      , (addrtot(&p->me.addr, 0, b1, sizeof(b1)), b1)
		      , p->me.host_port
		      , (addrtot(&p->him.addr, 0, b2, sizeof(b2)), b2)
		      , p->him.host_port));
		   
	if (sameaddr(&p->me.addr, myaddr)
	    && (!p->me.host_port_specific || p->me.host_port == myport)
	    && sameaddr(&p->him.addr, hisaddr)
	    && (!p->him.host_port_specific || p->him.host_port == hisport)
	    )
	{
	    if (prev != NULL)
	    {
		prev->next = p->next;	/* remove p from list */
		p->next = host_pairs;	/* and stick it on front */
		host_pairs = p;
	    }
	    break;
	}
    }
    return p;
}

/* find head of list of connections with this pair of hosts */
static struct connection *
find_host_pair_connections(const char *func
			   , const ip_address *myaddr, u_int16_t myport			   
			   , const ip_address *hisaddr, u_int16_t hisport)
{
    char b1[ADDRTOT_BUF],b2[ADDRTOT_BUF];
    struct host_pair *hp = find_host_pair(myaddr, myport, hisaddr, hisport);

    DBG(DBG_CONTROLMORE
	, DBG_log("find_host_pair_conn (%s): %s:%d %s:%d -> hp:%s\n"
		  , func
		  , (addrtot(myaddr,  0, b1, sizeof(b1)), b1)
		  , myport
		  , hisaddr ? (addrtot(hisaddr, 0, b2, sizeof(b2)), b2) : "%any"
		  , hisport
		  , (hp && hp->connections) ? hp->connections->name : "none"));
		   
    return hp == NULL? NULL : hp->connections;
}

static void
connect_to_host_pair(struct connection *c)
{
    if (oriented(*c))
    {
	struct host_pair *hp = find_host_pair(&c->spd.this.host_addr
					      , c->spd.this.host_port
					      , &c->spd.that.host_addr
					      , c->spd.that.host_port);

	char b1[ADDRTOT_BUF],b2[ADDRTOT_BUF];

	DBG(DBG_CONTROLMORE
	    , DBG_log("connect_to_host_pair: %s:%d %s:%d -> hp:%s\n"
		      , (addrtot(&c->spd.this.host_addr, 0, b1,sizeof(b1)), b1)
		      , c->spd.this.host_port
		      , (addrtot(&c->spd.that.host_addr, 0, b2,sizeof(b2)), b2)
		      , c->spd.that.host_port
		      , (hp && hp->connections) ? hp->connections->name : "none"));
		   
	if (hp == NULL)
	{
	    /* no suitable host_pair -- build one */
	    hp = alloc_thing(struct host_pair, "host_pair");
	    hp->me.addr = c->spd.this.host_addr;
	    hp->him.addr = c->spd.that.host_addr;
#ifdef NAT_TRAVERSAL
	    hp->me.host_port = nat_traversal_enabled ? pluto_port : c->spd.this.host_port;
	    hp->him.host_port = nat_traversal_enabled ? pluto_port : c->spd.that.host_port;
#else
	    hp->me.host_port = c->spd.this.host_port;
 	    hp->him.host_port = c->spd.that.host_port;
#endif
	    hp->initial_connection_sent = FALSE;
	    hp->connections = NULL;
	    hp->pending = NULL;
	    hp->next = host_pairs;
	    host_pairs = hp;
	}
	c->host_pair = hp;
	c->hp_next = hp->connections;
	hp->connections = c;
    }
    else
    {
	/* since this connection isn't oriented, we place it
	 * in the unoriented_connections list instead.
	 */
	c->host_pair = NULL;
	c->hp_next = unoriented_connections;
	unoriented_connections = c;
    }
}

/* find a connection by name.
 * If strict, don't accept a CK_INSTANCE.
 * Move the winner (if any) to the front.
 * If none is found, and strict, a diagnostic is logged to whack.
 */
struct connection *
con_by_name(const char *nm, bool strict)
{
    struct connection *p, *prev;

    for (prev = NULL, p = connections; ; prev = p, p = p->ac_next)
    {
	if (p == NULL)
	{
	    if (strict)
		whack_log(RC_UNKNOWN_NAME
		    , "no connection named \"%s\"", nm);
	    break;
	}
	if (streq(p->name, nm)
	&& (!strict || p->kind != CK_INSTANCE))
	{
	    if (prev != NULL)
	    {
		prev->ac_next = p->ac_next;	/* remove p from list */
		p->ac_next = connections;	/* and stick it on front */
		connections = p;
	    }
	    break;
	}
    }
    return p;
}

void
release_connection(struct connection *c, bool relations)
{
    if (c->kind == CK_INSTANCE)
    {
	/* This does everything we need.
	 * Note that we will be called recursively by delete_connection,
	 * but kind will be CK_GOING_AWAY.
	 */
	delete_connection(c, relations);
    }
    else
    {
	flush_pending_by_connection(c);
	delete_states_by_connection(c, relations);
	unroute_connection(c);
    }
}

/* Delete a connection */

#define list_rm(etype, enext, e, ehead) { \
	etype **ep; \
	for (ep = &(ehead); *ep != (e); ep = &(*ep)->enext) \
	    passert(*ep != NULL);    /* we must not come up empty-handed */ \
	*ep = (e)->enext; \
    }

#ifdef DYNAMICDNS
/* update the host pairs with the latest DNS ip address */
static void
update_host_pairs(struct connection *c)
{
    struct connection *d = NULL, *conn_next_tmp = NULL, *conn_list = NULL;
    struct host_pair *p = NULL, *hp_next_tmp;
    ip_address new_addr;
    char *dnshostname;

    d = c->host_pair->connections;
    p = c->host_pair;

    if (d == NULL
    	|| p == NULL
	|| d->dnshostname == NULL
	|| ttoaddr(d->dnshostname, 0, d->addr_family, &new_addr) != NULL
	|| sameaddr(&new_addr, &p->him.addr)) 
	    return;

    /* remember this dnshostname */
    dnshostname = c->dnshostname;

    for (; d != NULL; d = hp_next_tmp)
    {
	hp_next_tmp = d->hp_next;
	if (d->dnshostname && strcmp(d->dnshostname, dnshostname) == 0)
	{
	      /* 
	      * If there is a dnshostname and it is the same as the one that has changed, then 
	      * change the connection's remote host address and remove the connection from the host pair.
	      */
	      d->spd.that.host_addr = new_addr;
	      list_rm(struct connection, hp_next, d, d->host_pair->connections);

	      d->hp_next = conn_list;
	      conn_list = d;
	}
    }

    if (conn_list)
    {
	d = conn_list;
	for (; d != NULL; d = conn_next_tmp)
	{
	    /* 
	    * connect the connection to the new host_pair
	    */
	    conn_next_tmp = d->hp_next;
	    connect_to_host_pair(d);
	}
    }

    if (p->connections == NULL)
    {
	passert(p->pending == NULL);	/* ??? must deal with this! */
	list_rm(struct host_pair, next, p, host_pairs);
	pfree(p);
    }
}
#endif /* DYNAMICDNS */

static void
delete_end(struct connection *c UNUSED, struct spd_route *sr UNUSED, struct end *e)
{
    free_id_content(&e->id);
    pfreeany(e->updown);
    freeanychunk(e->ca);
#ifdef SMARTCARD
    scx_release(e->sc);
#endif
    release_cert(e->cert);
    free_ietfAttrList(e->groups);
}

static void
delete_sr(struct connection *c, struct spd_route *sr)
{
    delete_end(c, sr, &sr->this);
    delete_end(c, sr, &sr->that);
}


void
delete_connection(struct connection *c, bool relations)
{
    struct spd_route *sr;
    struct connection *old_cur_connection
	= cur_connection == c? NULL : cur_connection;
#ifdef DEBUG
    lset_t old_cur_debugging = cur_debugging;
#endif
	union {
		struct alg_info**     ppai;
#ifdef KERNEL_ALG
		struct alg_info_esp** ppai_esp;
#endif
#ifdef IKE_ALG
		struct alg_info_ike** ppai_ike;
#endif
	} palg_info;

    set_cur_connection(c);

    /* Must be careful to avoid circularity:
     * we mark c as going away so it won't get deleted recursively.
     */
    passert(c->kind != CK_GOING_AWAY);
    if (c->kind == CK_INSTANCE)
    {
	openswan_log("deleting connection \"%s\" instance with peer %s {isakmp=#%lu/ipsec=#%lu}"
	     , c->name
	     , ip_str(&c->spd.that.host_addr)
	     , c->newest_isakmp_sa, c->newest_ipsec_sa);
	c->kind = CK_GOING_AWAY;
    }
    else
    {
	openswan_log("deleting connection");
    }
    release_connection(c, relations);	/* won't delete c */

    if (c->kind == CK_GROUP)
	delete_group(c);

    /* free up any logging resources */
    perpeer_logfree(c);

    /* find and delete c from connections list */
    list_rm(struct connection, ac_next, c, connections);
    cur_connection = old_cur_connection;

    /* find and delete c from the host pair list */
    if (c->host_pair == NULL)
    {
	list_rm(struct connection, hp_next, c, unoriented_connections);
    }
    else
    {
	struct host_pair *hp = c->host_pair;

	list_rm(struct connection, hp_next, c, hp->connections);
	c->host_pair = NULL;	/* redundant, but safe */

	/* if there are no more connections with this host_pair
	 * and we haven't even made an initial contact, let's delete
	 * this guy in case we were created by an attempted DOS attack.
	 */
	if (hp->connections == NULL
	&& !hp->initial_connection_sent)
	{
	    passert(hp->pending == NULL);	/* ??? must deal with this! */
	    list_rm(struct host_pair, next, hp, host_pairs);
	    pfree(hp);
	}
    }

#ifdef VIRTUAL_IP
    if (c->kind != CK_GOING_AWAY) pfreeany(c->spd.that.virt);
#endif

#ifdef DEBUG
    set_debugging(old_cur_debugging);
#endif
    pfreeany(c->name);
#ifdef DYNAMICDNS
    pfreeany(c->dnshostname);
#endif /* DYNAMICDNS */

    sr = &c->spd;
    while(sr) {
	delete_sr(c, sr);
	sr = sr->next;
    }

    free_generalNames(c->requested_ca, TRUE);

    gw_delref(&c->gw_info);
#ifdef KERNEL_ALG
    palg_info.ppai_esp = &c->alg_info_esp;
    alg_info_delref(palg_info.ppai);
    /* causes double free errors
     * pfree(c->alg_esp); 
     */
#endif
#ifdef IKE_ALG
    palg_info.ppai_ike = &c->alg_info_ike;
    alg_info_delref(palg_info.ppai);
    /* causes double free errors
     * pfree(c->alg_ike); 
     */
#endif
    pfree(c);
}

/* Delete connections with the specified name */
void
delete_connections_by_name(const char *name, bool strict)
{
    struct connection *c = con_by_name(name, strict);

    for (; c != NULL; c = con_by_name(name, FALSE))
	delete_connection(c, FALSE);
}

void
delete_every_connection(void)
{
    while (connections != NULL)
	delete_connection(connections, TRUE);
}

void
release_dead_interfaces(void)
{
    struct host_pair *hp;

    for (hp = host_pairs; hp != NULL; hp = hp->next)
    {
	struct connection **pp
	    , *p;

	for (pp = &hp->connections; (p = *pp) != NULL; )
	{
	    if (p->interface->change == IFN_DELETE)
	    {
		/* this connection's interface is going away */
		enum connection_kind k = p->kind;

		release_connection(p, TRUE);

		if (k <= CK_PERMANENT)
		{
		    /* The connection should have survived release:
		     * move it to the unoriented_connections list.
		     */
		    passert(p == *pp);

		    p->interface = NULL;

		    *pp = p->hp_next;	/* advance *pp */
		    p->host_pair = NULL;
		    p->hp_next = unoriented_connections;
		    unoriented_connections = p;
		}
		else
		{
		    /* The connection should have vanished,
		     * but the previous connection remains.
		     */
		    passert(p != *pp);
		}
	    }
	    else
	    {
		pp = &p->hp_next;	/* advance pp */
	    }
	}
    }
}

/* adjust orientations of connections to reflect newly added interfaces */
void
check_orientations(void)
{
    /* try to orient all the unoriented connections */
    {
	struct connection *c = unoriented_connections;

	unoriented_connections = NULL;

	while (c != NULL)
	{
	    struct connection *nxt = c->hp_next;

	    (void)orient(c);
	    connect_to_host_pair(c);
	    c = nxt;
	}
    }

    /* Check that no oriented connection has become double-oriented.
     * In other words, the far side must not match one of our new interfaces.
     */
    {
	struct iface_port *i;

	for (i = interfaces; i != NULL; i = i->next)
	{
	    if (i->change == IFN_ADD)
	    {
		struct host_pair *hp;

		for (hp = host_pairs; hp != NULL; hp = hp->next)
		{
		    if (sameaddr(&hp->him.addr, &i->ip_addr)
			&& (kern_interface!=NO_KERNEL || hp->him.host_port == pluto_port))
		    {
			/* bad news: the whole chain of connections
			 * hanging off this host pair has both sides
			 * matching an interface.
			 * We'll get rid of them, using orient and
			 * connect_to_host_pair.  But we'll be lazy
			 * and not ditch the host_pair itself (the
			 * cost of leaving it is slight and cannot
			 * be induced by a foe).
			 */
			struct connection *c = hp->connections;

			hp->connections = NULL;
			while (c != NULL)
			{
			    struct connection *nxt = c->hp_next;

			    c->interface = NULL;
			    (void)orient(c);
			    connect_to_host_pair(c);
			    c = nxt;
			}
		    }
		}
	    }
	}
    }
}

static err_t
default_end(struct end *e, ip_address *dflt_nexthop)
{
    err_t ugh = NULL;
    const struct af_info *afi = aftoinfo(addrtypeof(&e->host_addr));

    if (afi == NULL)
	return "unknown address family in default_end";

    /* default ID to IP (but only if not NO_IP -- WildCard) */
    if (e->id.kind == ID_NONE && !isanyaddr(&e->host_addr))
    {
	e->id.kind = afi->id_addr;
	e->id.ip_addr = e->host_addr;
	e->has_id_wildcards = FALSE;
    }

    /* default nexthop to other side */
    if (isanyaddr(&e->host_nexthop))
	e->host_nexthop = *dflt_nexthop;

    /* default client to subnet containing only self
     * XXX This may mean that the client's address family doesn't match
     * tunnel_addr_family.
     */
    if (!e->has_client)
	ugh = addrtosubnet(&e->host_addr, &e->client);

    if(e->sendcert == 0) {
	e->sendcert = cert_sendifasked;
    }

    return ugh;
}

/* Format the topology of a connection end, leaving out defaults.
 * Largest left end looks like: client === host : port [ host_id ] --- hop
 * Note: if that==NULL, skip nexthop
 * Returns strlen of formated result (length excludes NUL at end).
 */
size_t
format_end(char *buf
	   , size_t buf_len
	   , const struct end *this
	   , const struct end *that
	   , bool is_left
	   , lset_t policy)
{
    char client[SUBNETTOT_BUF];
    const char *client_sep = "";
    char protoport[sizeof(":255/65535")];
    const char *host = NULL;
    char host_space[ADDRTOT_BUF];
    char host_port[sizeof(":65535")];
    char host_id[IDTOA_BUF + 2];
    char hop[ADDRTOT_BUF];
    char endopts[sizeof("MS+MC+XS+XC")+1];
    const char *hop_sep = "";
    const char *open_brackets  = "";
    const char *close_brackets = "";
    const char *id_obrackets = "";
    const char *id_cbrackets = "";
    const char *id_comma = "";

    memset(endopts, 0, sizeof(endopts));

    if (isanyaddr(&this->host_addr))
    {
	switch (policy & (POLICY_GROUP | POLICY_OPPO))
	{
	case POLICY_GROUP:
	    host = "%group";
	    break;
	case POLICY_OPPO:
	    host = "%opportunistic";
	    break;
	case POLICY_GROUP | POLICY_OPPO:
	    host = "%opportunisticgroup";
	    break;
	default:
	    host = "%any";
	    break;
	}
    }

    client[0] = '\0';

#ifdef VIRTUAL_IP
    if (is_virtual_end(this) && isanyaddr(&this->host_addr)) {
	host = "%virtual";
    }
#endif

    /* [client===] */
    if (this->has_client)
    {
	ip_address client_net, client_mask;

	networkof(&this->client, &client_net);
	maskof(&this->client, &client_mask);
	client_sep = "===";

 	/* {client_subnet_wildcard} */
 	if (this->has_client_wildcard)
 	{
 	    open_brackets  = "{";
 	    close_brackets = "}";
 	}

	if (isanyaddr(&client_net) && isanyaddr(&client_mask)
	&& (policy & (POLICY_GROUP | POLICY_OPPO)))
	    client_sep = "";	/* boring case */
	else if (subnetisnone(&this->client))
	    strcpy(client, "?");
	else
	    subnettot(&this->client, 0, client, sizeof(client));
    }

    /* host */
    if (host == NULL)
    {
	addrtot(&this->host_addr, 0, host_space, sizeof(host_space));
	host = host_space;
    }

    host_port[0] = '\0';
    if (this->host_port_specific)
	snprintf(host_port, sizeof(host_port), ":%u"
	    , this->host_port);

    /* payload portocol and port */
    protoport[0] = '\0';
    if (this->has_port_wildcard)
	snprintf(protoport, sizeof(protoport), ":%u/%%any", this->protocol);
    else if (this->port || this->protocol)
	snprintf(protoport, sizeof(protoport), ":%u/%u", this->protocol
	    , this->port);

    /* id, if different from host */
    host_id[0] = '\0';
    if (this->id.kind == ID_MYID)
    {
	id_obrackets = "[";
	id_cbrackets = "]";
	strcpy(host_id, "%myid");
    }
    else if (!(this->id.kind == ID_NONE
    || (id_is_ipaddr(&this->id) && sameaddr(&this->id.ip_addr, &this->host_addr))))
    {
	id_obrackets = "[";
	id_cbrackets = "]";
	idtoa(&this->id, host_id, sizeof(host_id));
    }

    if(this->modecfg_server || this->modecfg_client
       || this->xauth_server || this->xauth_client
       || this->sendcert != cert_defaultcertpolicy)
    {
	const char *plus = "";
	endopts[0]='\0';

	if(id_obrackets && id_obrackets[0]=='[')
	{
	    id_comma=",";
	} else {
	    id_obrackets = "[";
	    id_cbrackets = "]";
	}

	if(this->modecfg_server) {
	    strncat(endopts, plus, sizeof(endopts));
	    strncat(endopts, "MS", sizeof(endopts));
	    plus="+";
	}

	if(this->modecfg_client) {
	    strncat(endopts, plus, sizeof(endopts));
	    strncat(endopts, "MC", sizeof(endopts));
	    plus="+";
	}

	if(this->xauth_server) {
	    strncat(endopts, plus, sizeof(endopts));
	    strncat(endopts, "XS", sizeof(endopts));
	    plus="+";
	}
	    
	if(this->xauth_client) {
	    strncat(endopts, plus, sizeof(endopts));
	    strncat(endopts, "XC", sizeof(endopts));
	    plus="+";
	}

	{
	    const char *send = "";
	    char s[32];

	    send="";
	    
	    switch(this->sendcert) {
	    case cert_neversend:
		send="S-C";
		break;
	    case cert_sendifasked:
		send="S?C";
		break;
	    case cert_alwayssend:
		send="S=C";
		break;
	    case cert_forcedtype:
		sprintf(s, "S%d", this->cert.type);
		send=s;
		break;
	    }
	    strncat(endopts, plus, sizeof(endopts));
	    strncat(endopts, send, sizeof(endopts));
	    plus="+";
	}
    }
	    
    /* [---hop] */
    hop[0] = '\0';
    hop_sep = "";
    if (that != NULL && !sameaddr(&this->host_nexthop, &that->host_addr))
    {
	addrtot(&this->host_nexthop, 0, hop, sizeof(hop));
	hop_sep = "---";
    }

    if (is_left)
	snprintf(buf, buf_len, "%s%s%s%s%s%s%s%s%s%s%s%s%s%s"
	    , open_brackets, client, close_brackets
	    , client_sep, host, host_port
		 , id_obrackets, host_id, id_comma, endopts, id_cbrackets
	    , protoport, hop_sep, hop);
    else
	snprintf(buf, buf_len, "%s%s%s%s%s%s%s%s%s%s%s%s%s%s"
	    , hop, hop_sep, host, host_port
		 , id_obrackets, host_id, id_comma, endopts, id_cbrackets
	    , protoport, client_sep
	    , open_brackets, client, close_brackets);
    return strlen(buf);
}

/* format topology of a connection.
 * Two symmetric ends separated by ...
 */
#define CONNECTION_BUF	(2 * (END_BUF - 1) + 4)

static size_t
format_connection(char *buf, size_t buf_len
		  , const struct connection *c
		  , struct spd_route *sr)
{
    size_t w = format_end(buf, buf_len, &sr->this, &sr->that, TRUE, LEMPTY);

    w += snprintf(buf + w, buf_len - w, "...");
    return w + format_end(buf + w, buf_len - w, &sr->that, &sr->this, FALSE, c->policy);
}

static void
unshare_connection_strings(struct connection *c)
{
    c->name = clone_str(c->name, "connection name");
#ifdef DYNAMICDNS
    c->dnshostname = clone_str(c->dnshostname, "connection dnshostname");
#endif /* DYNAMICDNS */

    unshare_id_content(&c->spd.this.id);
    c->spd.this.updown = clone_str(c->spd.this.updown, "updown");

#ifdef SMARTCARD
    scx_share(c->spd.this.sc);
#endif
    share_cert(c->spd.this.cert);
    if (c->spd.this.ca.ptr != NULL)
	clonetochunk(c->spd.this.ca, c->spd.this.ca.ptr, c->spd.this.ca.len, "ca string");

    unshare_id_content(&c->spd.that.id);
    c->spd.that.updown = clone_str(c->spd.that.updown, "updown");

#ifdef SMARTCARD
    scx_share(c->spd.that.sc);
#endif
    share_cert(c->spd.that.cert);
    if (c->spd.that.ca.ptr != NULL)
	clonetochunk(c->spd.that.ca, c->spd.that.ca.ptr, c->spd.that.ca.len, "ca string");

    /* increment references to algo's, if any */
    if(c->alg_info_ike) {
	alg_info_addref(IKETOINFO(c->alg_info_ike));
    }

    if(c->alg_info_esp) {
	alg_info_addref(ESPTOINFO(c->alg_info_esp));
    }

}

static void
load_end_certificate(const char *filename, struct end *dst)
{
    time_t valid_until;
    cert_t cert;
    bool valid_cert = FALSE;
    bool cached_cert = FALSE;

    memset(&dst->cert, 0, sizeof(dst->cert));

    /* initialize end certificate */
    dst->cert.type = CERT_NONE;

    /* initialize smartcard info record */
    dst->sc = NULL;

    if (filename != NULL)
    {
#ifdef SMARTCARD
	if (strncmp(filename, SCX_TOKEN, strlen(SCX_TOKEN)) == 0)
	{
	    /* we have a smartcard */
	    smartcard_t *sc = scx_parse_reader_id(filename + strlen(SCX_TOKEN));
	    dst->sc = scx_add(sc);

	    /* is there a cached smartcard certificate? */
	    cached_cert = dst->sc->last_cert.type != CERT_NONE
		&& (time(NULL) - dst->sc->last_load) < SCX_CERT_CACHE_INTERVAL;

	    if (cached_cert)
	    {
		cert = dst->sc->last_cert;
		valid_cert = TRUE;
	    }
	    else
	    	valid_cert = scx_load_cert(dst->sc, &cert);
	}
	else
#endif
	{
	    /* load cert from file */
	    valid_cert = load_host_cert(FALSE, filename, &cert);
	}
    }

    if (valid_cert)
    {
	err_t ugh = NULL;

	switch (cert.type)
	{
	case CERT_PGP:
	    select_pgpcert_id(cert.u.pgp, &dst->id);

	    if (cached_cert)
		dst->cert = cert;
	    else
	    {
		valid_until = cert.u.pgp->until;
		add_pgp_public_key(cert.u.pgp, cert.u.pgp->until, DAL_LOCAL);
		dst->cert.type = cert.type;
		dst->cert.u.pgp = add_pgpcert(cert.u.pgp);
	    }
	    break;

	case CERT_X509_SIGNATURE:
	    select_x509cert_id(cert.u.x509, &dst->id);

	    if (!cached_cert)
	    {
		/* check validity of cert */
		valid_until = cert.u.x509->notAfter;
		ugh = check_validity(cert.u.x509, &valid_until);
	    }
	    if (ugh != NULL)
	    {
		openswan_log("  %s", ugh);
		free_x509cert(cert.u.x509);
	    }
	    else
	    {
	    	DBG(DBG_CONTROL,
		    DBG_log("certificate is valid")
		)
		if (cached_cert)
		    dst->cert = cert;
		else
		{
		    add_x509_public_key(cert.u.x509, valid_until, DAL_LOCAL);
		    dst->cert.type = cert.type;
		    dst->cert.u.x509 = add_x509cert(cert.u.x509);
		}
		/* if no CA is defined, use issuer as default */
		if (dst->ca.ptr == NULL)
		    dst->ca = dst->cert.u.x509->issuer;
	    }
	    break;
	default:
	    break;
	}

	/* cache the certificate that was last retrieved from the smartcard */
	if (dst->sc != NULL)
	{
	    if (!same_cert(&dst->sc->last_cert, &dst->cert))
	    {
		release_cert(dst->sc->last_cert);
		dst->sc->last_cert = dst->cert;
		share_cert(dst->cert);
	    }
	    time(&dst->sc->last_load);
	}
    }
}

static bool
extract_end(struct end *dst, const struct whack_end *src, const char *which)
{
    bool same_ca = FALSE;

    /* decode id, if any */
    if (src->id == NULL)
    {
	dst->id.kind = ID_NONE;
    }
    else
    {
	err_t ugh = atoid(src->id, &dst->id, TRUE);

	if (ugh != NULL)
	{
	    loglog(RC_BADID, "bad %s --id: %s (ignored)", which, ugh);
	    dst->id = empty_id;	/* ignore bad one */
	}
    }

    dst->ca = empty_chunk;

    /* decode CA distinguished name, if any */
    if (src->ca != NULL)
    {
	if streq(src->ca, "%same")
	    same_ca = TRUE;
	else if (!streq(src->ca, "%any"))
	{
	    err_t ugh;

	    dst->ca.ptr = temporary_cyclic_buffer();
	    ugh = atodn(src->ca, &dst->ca);
	    if (ugh != NULL)
	    {
		openswan_log("bad CA string '%s': %s (ignored)", src->ca, ugh);
		dst->ca = empty_chunk;
	    }
	}
    }

    if(src->sendcert == cert_forcedtype) {
	/* certificate is a blob */
	dst->cert.forced = TRUE;
	dst->cert.type = src->certtype;
	load_cert(TRUE, src->cert, "forced cert", &dst->cert);
    } else {
	/* load local end certificate and extract ID, if any */
	load_end_certificate(src->cert, dst);
    }

    /* does id has wildcards? */
    dst->has_id_wildcards = id_count_wildcards(&dst->id) > 0;

    /* decode group attributes, if any */
    decode_groups(src->groups, &dst->groups);

    /* the rest is simple copying of corresponding fields */
    dst->host_addr = src->host_addr;
    dst->host_nexthop = src->host_nexthop;
    dst->host_srcip = src->host_srcip;
    dst->client = src->client;
#ifdef MODECFG
    dst->modecfg_server = src->modecfg_server;
    dst->modecfg_client = src->modecfg_client;
#endif
#ifdef XAUTH
    dst->xauth_server = src->xauth_server;
    dst->xauth_client = src->xauth_client;
#endif
    dst->protocol = src->protocol;
    dst->port = src->port;
    dst->has_port_wildcard = src->has_port_wildcard;
    dst->key_from_DNS_on_demand = src->key_from_DNS_on_demand;
    dst->has_client = src->has_client;
    dst->has_client_wildcard = src->has_client_wildcard;
    dst->updown = src->updown;
    dst->host_port = IKE_UDP_PORT;
    if(src->host_port != IKE_UDP_PORT) {
	dst->host_port = src->host_port;
	dst->host_port_specific = TRUE;
    }

    dst->sendcert =  src->sendcert;
    
    return same_ca;
}

static bool
check_connection_end(const struct whack_end *this, const struct whack_end *that
, const struct whack_message *wm)
{
    if (wm->addr_family != addrtypeof(&this->host_addr)
    || wm->addr_family != addrtypeof(&this->host_nexthop)
    || (this->has_client? wm->tunnel_addr_family : wm->addr_family)
      != subnettypeof(&this->client)
    || subnettypeof(&this->client) != subnettypeof(&that->client))
    {
	/* this should have been diagnosed by whack, so we need not be clear
	 * !!! overloaded use of RC_CLASH
	 */
	loglog(RC_CLASH, "address family inconsistency in connection");
	return FALSE;
    }

    if (isanyaddr(&that->host_addr))
    {
	/* other side is wildcard: we must check if other conditions met */
	if (isanyaddr(&this->host_addr))
	{
	    loglog(RC_ORIENT, "connection must specify host IP address for our side");
	    return FALSE;
	}
	else if (!NEVER_NEGOTIATE(wm->policy))
	{
	    /* check that all main mode RW IKE policies agree because we must
	     * implement them before the correct connection is known.
	     * We cannot enforce this for other non-RW connections because
	     * differentiation is possible when a command specifies which
	     * to initiate.
	     * aggressive mode IKE policies do not have to agree amongst
	     * themselves as the ID is known from the outset.
	     */
	    const struct connection *c = NULL;

	    c = find_host_pair_connections(__FUNCTION__
					   , &this->host_addr
					   , this->host_port
					   , (const ip_address *)NULL
					   , that->host_port);

	    for (; c != NULL; c = c->hp_next)
	    {
		if (c->policy & POLICY_AGGRESSIVE)
			continue;
		if (!NEVER_NEGOTIATE(c->policy)
		&& ((c->policy ^ wm->policy) & (POLICY_PSK | POLICY_RSASIG)))
		{
		    loglog(RC_CLASH
			, "authentication method disagrees with \"%s\", which is also for an unspecified peer"
			, c->name);
		    return FALSE;
		}
	    }
	}
    }
#ifdef VIRTUAL_IP
    if ((this->virt) && (!isanyaddr(&this->host_addr) || this->has_client)) {
	loglog(RC_CLASH,
	    "virtual IP must only be used with %%any and without client");
	return FALSE;
    }
#endif    
    return TRUE;	/* happy */
}

struct connection *
find_connection_by_reqid(uint32_t reqid)
{
    struct connection *c;

    reqid &= ~3;
    for (c = connections; c != NULL; c = c->ac_next)
    {
	if (c->spd.reqid == reqid)
	    return c;
    }

    return NULL;
}

static uint32_t
gen_reqid(void)
{
    uint32_t start;
    static uint32_t reqid = IPSEC_MANUAL_REQID_MAX & ~3;

    start = reqid;
    do {
	reqid += 4;
	if (reqid == 0)
	    reqid = (IPSEC_MANUAL_REQID_MAX & ~3) + 4;
	if (!find_connection_by_reqid(reqid))
	    return reqid;
    } while (reqid != start);

    exit_log("unable to allocate reqid");
}

void
add_connection(const struct whack_message *wm)
{
    struct alg_info_ike *alg_info_ike;
    const char *ugh;

    alg_info_ike = NULL;

    if (con_by_name(wm->name, FALSE) != NULL)
    {
	loglog(RC_DUPNAME, "attempt to redefine connection \"%s\"", wm->name);
    }
    else if (wm->right.protocol != wm->left.protocol)
    {
	/* this should haven been diagnosed by whack
	 * !!! overloaded use of RC_CLASH
	 */
	loglog(RC_CLASH, "the protocol must be the same for leftport and rightport");
    }
    else if(wm->ike != NULL
	    && ((alg_info_ike = alg_info_ike_create_from_str(wm->ike, &ugh))==NULL
		|| alg_info_ike->alg_info_cnt==0)) {
	if (alg_info_ike!= NULL && alg_info_ike->alg_info_cnt==0) {
	    loglog(RC_NOALGO
		   , "got 0 transforms for ike=\"%s\""
		   , wm->esp);
	    return;
	} 

	loglog(RC_NOALGO
	       , "esp string error: %s"
	       , ugh? ugh : "Unknown");
	return;
    }
    else if ((wm->ike == NULL || alg_info_ike != NULL)
	     && check_connection_end(&wm->right, &wm->left, wm)
	     && check_connection_end(&wm->left, &wm->right, wm))
    {
	bool same_rightca, same_leftca;
	struct connection *c = alloc_thing(struct connection, "struct connection");

	same_rightca = same_leftca = FALSE;
	c->name = wm->name;
#ifdef DYNAMICDNS
	if (wm->dnshostname)
		c->dnshostname = wm->dnshostname;
#endif /* DYNAMICDNS */

	c->policy = wm->policy;

	DBG(DBG_CONTROL,
	    DBG_log("Added new connection %s with policy %s"
		    , c->name
		    , prettypolicy(c->policy)));
    
	if ((c->policy & POLICY_COMPRESS) && !can_do_IPcomp)
	    loglog(RC_COMMENT
		, "ignoring --compress in \"%s\" because KLIPS is not configured to do IPCOMP"
		, c->name);

	c->alg_info_esp = NULL;
	c->alg_esp = NULL;
#ifdef KERNEL_ALG
	if (wm->esp)  
	{
		DBG(DBG_CONTROL, DBG_log("from whack: got --esp=%s", wm->esp ? wm->esp: "NULL"));
		c->alg_info_esp = alg_info_esp_create_from_str(wm->esp? wm->esp : "", &ugh, FALSE);

		DBG(DBG_CRYPT|DBG_CONTROL, 
			static char buf[256]="<NULL>";
			if (c->alg_info_esp)
				alg_info_snprint(buf, sizeof(buf), 
						 (struct alg_info *)c->alg_info_esp, TRUE);
			DBG_log("esp string values: %s", buf);
		);
		if (c->alg_info_esp) {
			if (c->alg_info_esp->alg_info_cnt==0) {
				loglog(RC_NOALGO
					, "got 0 transforms for esp=\"%s\""
					, wm->esp);
				return;
			}
		} else {
			loglog(RC_NOALGO
				, "esp string error: %s"
				, ugh? ugh : "Unknown");
			return;
		}
		c->alg_esp = clone_str(wm->esp, "esp string");
	}
#endif	

	c->alg_info_ike = NULL;
	c->alg_ike = NULL;
#ifdef IKE_ALG
	if (wm->ike)
	{
	    c->alg_info_ike = alg_info_ike;

	    DBG(DBG_CONTROL, DBG_log("from whack: got --ike=%s", wm->ike));
	    DBG(DBG_CRYPT|DBG_CONTROL, 
		char buf[256];
		alg_info_snprint(buf, sizeof(buf),
				 (struct alg_info *)c->alg_info_ike, TRUE);
		DBG_log("ike string values: %s", buf);
		);
	    if (c->alg_info_ike) {
		if (c->alg_info_ike->alg_info_cnt==0) {
		    loglog(RC_NOALGO
			   , "got 0 transforms for ike=\"%s\""
			   , wm->ike);
		    return;
		}
	    } else {
		loglog(RC_NOALGO
		       , "ike string error: %s"
		       , ugh? ugh : "Unknown");
		return;
	    }
	    c->alg_ike = clone_str(wm->ike, "ike string");
	}
#endif
	c->sa_ike_life_seconds = wm->sa_ike_life_seconds;
	c->sa_ipsec_life_seconds = wm->sa_ipsec_life_seconds;
	c->sa_rekey_margin = wm->sa_rekey_margin;
	c->sa_rekey_fuzz = wm->sa_rekey_fuzz;
	c->sa_keying_tries = wm->sa_keying_tries;

	/* RFC 3706 DPD */
        c->dpd_delay = wm->dpd_delay;
        c->dpd_timeout = wm->dpd_timeout;
        c->dpd_action = wm->dpd_action;

	c->forceencaps = wm->forceencaps;

	c->addr_family = wm->addr_family;
	c->tunnel_addr_family = wm->tunnel_addr_family;

	c->requested_ca = NULL;

	same_leftca  = extract_end(&c->spd.this, &wm->left, "left");
	same_rightca = extract_end(&c->spd.that, &wm->right, "right");

	if (same_rightca)
	    c->spd.that.ca = c->spd.this.ca;
	else if (same_leftca)
	    c->spd.this.ca = c->spd.that.ca;

	default_end(&c->spd.this, &c->spd.that.host_addr);
	default_end(&c->spd.that, &c->spd.this.host_addr);

	/* force any wildcard host IP address, any wildcard subnet
	 * or any wildcard ID to that end
	 */
	if (isanyaddr(&c->spd.this.host_addr) || c->spd.this.has_client_wildcard
	|| c->spd.this.has_port_wildcard || c->spd.this.has_id_wildcards)
	{
	    struct end t = c->spd.this;

	    c->spd.this = c->spd.that;
	    c->spd.that = t;
	}

	c->spd.next = NULL;
	c->spd.reqid = gen_reqid();

	/* set internal fields */
	c->instance_serial = 0;
	c->ac_next = connections;
	connections = c;
	c->interface = NULL;
	c->spd.routing = RT_UNROUTED;
	c->newest_isakmp_sa = SOS_NOBODY;
	c->newest_ipsec_sa = SOS_NOBODY;
	c->spd.eroute_owner = SOS_NOBODY;

	if (c->policy & POLICY_GROUP)
	{
	    c->kind = CK_GROUP;
	    add_group(c);
	}
	else if ((isanyaddr(&c->spd.that.host_addr) && !NEVER_NEGOTIATE(c->policy))
		|| c->spd.that.has_client_wildcard || c->spd.that.has_port_wildcard  
		|| ((c->policy & POLICY_SHUNT_MASK) == 0  && c->spd.that.has_id_wildcards ))
	{
	    DBG(DBG_CONTROL, DBG_log("based upon policy, the connection is a template."));

	    /* Opportunistic or Road Warrior or wildcard client subnet
	     * or wildcard ID */
	    c->kind = CK_TEMPLATE;
	}
	else
	{
	    c->kind = CK_PERMANENT;
	}
	set_policy_prio(c);	/* must be after kind is set */

#ifdef DEBUG
	c->extra_debugging = wm->debugging;
#endif

	c->gw_info = NULL;

#ifdef VIRTUAL_IP
	passert(!(wm->left.virt && wm->right.virt));
	if (wm->left.virt || wm->right.virt) {
	    passert(isanyaddr(&c->spd.that.host_addr));
	    c->spd.that.virt = create_virtual(c,
		wm->left.virt ? wm->left.virt : wm->right.virt);
	    if (c->spd.that.virt)
		c->spd.that.has_client = TRUE;
	}
#endif

	unshare_connection_strings(c);
#ifdef KERNEL_ALG
	alg_info_addref((struct alg_info *)c->alg_info_esp);
#endif
#ifdef IKE_ALG
	alg_info_addref((struct alg_info *)c->alg_info_ike);
#endif

	(void)orient(c);
	connect_to_host_pair(c);

	/* log all about this connection */
	openswan_log("added connection description \"%s\"", c->name);
	DBG(DBG_CONTROL,
	    char topo[CONNECTION_BUF];

	    (void) format_connection(topo, sizeof(topo), c, &c->spd);

	    DBG_log("%s", topo);

	    /* Make sure that address families can be correctly inferred
	     * from printed ends.
	     */
	    passert(c->addr_family == addrtypeof(&c->spd.this.host_addr)
		&& c->addr_family == addrtypeof(&c->spd.this.host_nexthop)
		&& (c->spd.this.has_client? c->tunnel_addr_family : c->addr_family)
		  == subnettypeof(&c->spd.this.client)

		&& c->addr_family == addrtypeof(&c->spd.that.host_addr)
		&& c->addr_family == addrtypeof(&c->spd.that.host_nexthop)
		&& (c->spd.that.has_client? c->tunnel_addr_family : c->addr_family)
		  == subnettypeof(&c->spd.that.client));

	    DBG_log("ike_life: %lus; ipsec_life: %lus; rekey_margin: %lus;"
		" rekey_fuzz: %lu%%; keyingtries: %lu; policy: %s"
		, (unsigned long) c->sa_ike_life_seconds
		, (unsigned long) c->sa_ipsec_life_seconds
		, (unsigned long) c->sa_rekey_margin
		, (unsigned long) c->sa_rekey_fuzz
		, (unsigned long) c->sa_keying_tries
		, prettypolicy(c->policy));
	);
    } else {
	loglog(RC_FATAL, "attempt to load incomplete connection");
    }

}

/* Derive a template connection from a group connection and target.
 * Similar to instantiate().  Happens at whack --listen.
 * Returns name of new connection.  May be NULL.
 * Caller is responsible for pfreeing.
 */
char *
add_group_instance(struct connection *group, const ip_subnet *target)
{
    char namebuf[100]
	, targetbuf[SUBNETTOT_BUF];
    struct connection *t;
    char *name = NULL;

    passert(group->kind == CK_GROUP);
    passert(oriented(*group));

    /* manufacture a unique name for this template */
    subnettot(target, 0, targetbuf, sizeof(targetbuf));
    snprintf(namebuf, sizeof(namebuf), "%s#%s", group->name, targetbuf);

    if (con_by_name(namebuf, FALSE) != NULL)
    {
	loglog(RC_DUPNAME, "group name + target yields duplicate name \"%s\""
	    , namebuf);
    }
    else
    {
	t = clone_thing(*group, "group instance");
	t->name = namebuf;
	unshare_connection_strings(t);
	name = clone_str(t->name, "group instance name");
	t->spd.that.client = *target;
	t->policy &= ~(POLICY_GROUP | POLICY_GROUTED);
	t->kind = isanyaddr(&t->spd.that.host_addr) && !NEVER_NEGOTIATE(t->policy)
	    ? CK_TEMPLATE : CK_INSTANCE;

	/* reset log file info */
	t->log_file_name = NULL;
	t->log_file = NULL;
	t->log_file_err = FALSE;

	t->spd.reqid = gen_reqid();

#ifdef VIRTUAL_IP
	if (t->spd.that.virt) {
	        DBG_log("virtual_ip not supported in group instance");
		t->spd.that.virt = NULL;	
	}
#endif

	/* add to connections list */
	t->ac_next = connections;
	connections = t;

	/* same host_pair as parent: stick after parent on list */
	group->hp_next = t;

	/* route if group is routed */
	if (group->policy & POLICY_GROUTED)
	{
	    if (!trap_connection(t))
		whack_log(RC_ROUTE, "could not route");
	}
    }
    return name;
}

/* an old target has disappeared for a group: delete instance */
void
remove_group_instance(const struct connection *group USED_BY_DEBUG
, const char *name)
{
    passert(group->kind == CK_GROUP);
    passert(oriented(*group));

    delete_connections_by_name(name, FALSE);
}

/* Common part of instantiating a Road Warrior or Opportunistic connection.
 * his_id can be used to carry over an ID discovered in Phase 1.
 * It must not disagree with the one in c, but if that is unspecified,
 * the new connection will use his_id.
 * If his_id is NULL, and c.that.id is uninstantiated (ID_NONE), the
 * new connection will continue to have an uninstantiated that.id.
 * Note: instantiation does not affect port numbers.
 *
 * Note that instantiate can only deal with a single SPD/eroute.
 */
static struct connection *
instantiate(struct connection *c, const ip_address *him
	    , const struct id *his_id)
{
    struct connection *d;
    int wildcards;

    passert(c->kind == CK_TEMPLATE);
    passert(c->spd.next == NULL);

    c->instance_serial++;
    d = clone_thing(*c, "temporary connection");
    if (his_id != NULL)
    {
	passert(match_id(his_id, &d->spd.that.id, &wildcards));
	d->spd.that.id = *his_id;
	d->spd.that.has_id_wildcards = FALSE;
    }
    unshare_connection_strings(d);
    unshare_ietfAttrList(&d->spd.this.groups);
    unshare_ietfAttrList(&d->spd.that.groups);


#ifdef KERNEL_ALG
    alg_info_addref((struct alg_info *)d->alg_info_esp);
#endif
#ifdef IKE_ALG
    alg_info_addref((struct alg_info *)d->alg_info_ike);
#endif

    d->kind = CK_INSTANCE;

    passert(oriented(*d));
    d->spd.that.host_addr = *him;
    setportof(htons(c->spd.that.port), &d->spd.that.host_addr);
    default_end(&d->spd.that, &d->spd.this.host_addr);

    /* We cannot guess what our next_hop should be, but if it was
     * explicitly specified as 0.0.0.0, we set it to be him.
     * (whack will not allow nexthop to be elided in RW case.)
     */
    default_end(&d->spd.this, &d->spd.that.host_addr);
    d->spd.next = NULL;
    d->spd.reqid = gen_reqid();

    /* set internal fields */
    d->ac_next = connections;
    connections = d;
    d->spd.routing = RT_UNROUTED;
    d->newest_isakmp_sa = SOS_NOBODY;
    d->newest_ipsec_sa = SOS_NOBODY;
    d->spd.eroute_owner = SOS_NOBODY;

    /* reset log file info */
    d->log_file_name = NULL;
    d->log_file = NULL;
    d->log_file_err = FALSE;

    connect_to_host_pair(d);

    return d;
}

struct connection *
rw_instantiate(struct connection *c
, const ip_address *him
, const ip_subnet *his_net
, const struct id *his_id)
{
    struct connection *d = instantiate(c, him, his_id);

#ifdef VIRTUAL_IP
    if (d && his_net && is_virtual_connection(c)) {
	d->spd.that.client = *his_net;
	d->spd.that.virt = NULL;
	if (subnetishost(his_net) && addrinsubnet(him, his_net))
	    d->spd.that.has_client = FALSE;
    }
#endif

    if (d->policy & POLICY_OPPO)
    {
	/* This must be before we know the client addresses.
	 * Fill in one that is impossible.  This prevents anyone else from
	 * trying to use this connection to get to a particular client
	 */
	d->spd.that.client = *aftoinfo(subnettypeof(&d->spd.that.client))->none;
    }
    DBG(DBG_CONTROL
	, DBG_log("instantiated \"%s\" for %s" , d->name, ip_str(him)));
    return d;
}

struct connection *
oppo_instantiate(struct connection *c
		 , const ip_address *him
		 , const struct id *his_id
		 , struct gw_info *gw
		 , const ip_address *our_client USED_BY_DEBUG
		 , const ip_address *peer_client)
{
    struct connection *d = instantiate(c, him, his_id);

    DBG(DBG_CONTROL,
	DBG_log("oppo instantiate d=%s from c=%s with c->routing %s, d->routing %s"
		, d->name, c->name
		, enum_name(&routing_story, c->spd.routing)
		, enum_name(&routing_story, d->spd.routing)));

    passert(d->spd.next == NULL);

    /* fill in our client side */
    if (d->spd.this.has_client)
    {
	/* there was a client in the abstract connection
	 * so we demand that the required client is within that subnet.
	 */
	passert(addrinsubnet(our_client, &d->spd.this.client));
	happy(addrtosubnet(our_client, &d->spd.this.client));
	/* opportunistic connections do not use port selectors */
	setportof(0, &d->spd.this.client.addr);
    }
    else
    {
	/* there was no client in the abstract connection
	 * so we demand that the required client be the host
	 */
	passert(sameaddr(our_client, &d->spd.this.host_addr));
    }

    /*
     * fill in peer's client side.
     * If the client is the peer, excise the client from the connection.
     */
    passert((d->policy & POLICY_OPPO)
	&& addrinsubnet(peer_client, &d->spd.that.client));
    happy(addrtosubnet(peer_client, &d->spd.that.client));

    /* opportunistic connections do not use port selectors */
    setportof(0, &d->spd.that.client.addr);

    if (sameaddr(peer_client, &d->spd.that.host_addr))
	d->spd.that.has_client = FALSE;

    passert(d->gw_info == NULL);
    gw_addref(gw);
    d->gw_info = gw;

    /* Adjust routing if something is eclipsing c.
     * It must be a %hold for us (hard to passert this).
     * If there was another instance eclipsing, we'd be using it.
     */
    if (c->spd.routing == RT_ROUTED_ECLIPSED)
	d->spd.routing = RT_ROUTED_PROSPECTIVE;

    /* Remember if the template is routed:
     * if so, this instance applies for initiation
     * even if it is created for responding.
     */
    if (routed(c->spd.routing))
	d->instance_initiation_ok = TRUE;

    DBG(DBG_CONTROL,
	char topo[CONNECTION_BUF];

	(void) format_connection(topo, sizeof(topo), d, &d->spd);
	DBG_log("instantiated \"%s\": %s", d->name, topo);
    );
    return d;
}

/* priority formatting */
void
fmt_policy_prio(policy_prio_t pp, char buf[POLICY_PRIO_BUF])
{
    if (pp == BOTTOM_PRIO)
	snprintf(buf, POLICY_PRIO_BUF, "0");
    else
	snprintf(buf, POLICY_PRIO_BUF, "%lu,%lu"
	    , pp>>16, (pp & ~(~(policy_prio_t)0 << 16)) >> 8);
}

/* Format any information needed to identify an instance of a connection.
 * Fills any needed information into buf which MUST be big enough.
 * Road Warrior: peer's IP address
 * Opportunistic: [" " myclient "==="] " ..." peer ["===" hisclient] '\0'
 */
static size_t
fmt_client(const ip_subnet *client, const ip_address *gw, const char *prefix, char buf[ADDRTOT_BUF])
{
    if (subnetisaddr(client, gw))
    {
	buf[0] = '\0';	/* compact denotation for "self" */
    }
    else
    {
	char *ap;

	strcpy(buf, prefix);
	ap = buf + strlen(prefix);
	if (subnetisnone(client))
	    strcpy(ap, "?");	/* unknown */
	else
	    subnettot(client, 0, ap, SUBNETTOT_BUF);
    }
    return strlen(buf);
}

void
fmt_conn_instance(const struct connection *c, char buf[CONN_INST_BUF])
{
    char *p = buf;

    *p = '\0';

    if (c->kind == CK_INSTANCE)
    {
	if (c->instance_serial != 0)
	{
	    snprintf(p, CONN_INST_BUF, "[%lu]", c->instance_serial);
	    p += strlen(p);
	}

	if (c->policy & POLICY_OPPO)
	{
	    size_t w = fmt_client(&c->spd.this.client, &c->spd.this.host_addr, " ", p);

	    p += w;

	    strcpy(p, w == 0? " ..." : "=== ...");
	    p += strlen(p);

	    addrtot(&c->spd.that.host_addr, 0, p, ADDRTOT_BUF);
	    p += strlen(p);

	    (void) fmt_client(&c->spd.that.client, &c->spd.that.host_addr, "===", p);
	}
	else
	{
	    *p++ = ' ';
	    addrtot(&c->spd.that.host_addr, 0, p, ADDRTOT_BUF);
	}
    }
}

/* Find an existing connection for a trapped outbound packet.
 * This is attempted before we bother with gateway discovery.
 *   + this connection is routed or instance_of_routed_template
 *     (i.e. approved for on-demand)
 *   + this subnet contains our_client (or we are our_client)
 *   + that subnet contains peer_client (or peer is peer_client)
 *   + don't care about Phase 1 IDs (we don't know)
 * Note: result may still need to be instantiated.
 * The winner has the highest policy priority.
 *
 * If there are several with that priority, we give preference to
 * the first one that is an instance.
 *
 * See also build_outgoing_opportunistic_connection.
 */
struct connection *
find_connection_for_clients(struct spd_route **srp,
			    const ip_address *our_client,
			    const ip_address *peer_client,
			    int transport_proto)
{
    struct connection *c = connections, *best = NULL;
    policy_prio_t best_prio = BOTTOM_PRIO;
    struct spd_route *sr;
    struct spd_route *best_sr;
    int our_port  = ntohs(portof(our_client));
    int peer_port = ntohs(portof(peer_client));

    best_sr = NULL;

    passert(!isanyaddr(our_client) && !isanyaddr(peer_client));
#ifdef DEBUG
    if (DBGP(DBG_CONTROL))
    {
	char ocb[ADDRTOT_BUF], pcb[ADDRTOT_BUF];

	addrtot(our_client, 0, ocb, sizeof(ocb));
	addrtot(peer_client, 0, pcb, sizeof(pcb));
	DBG_log("find_connection: "
		"looking for policy for connection: %s:%d/%d -> %s:%d/%d"
		, ocb, transport_proto, our_port, pcb, transport_proto, peer_port);
    }
#endif /* DEBUG */

    for (c = connections; c != NULL; c = c->ac_next)
    {
	if (c->kind == CK_GROUP)
	    continue;

	for (sr = &c->spd; best!=c && sr; sr = sr->next)
	{
	    if ((routed(sr->routing) || c->instance_initiation_ok)
	    && addrinsubnet(our_client, &sr->this.client)
 	    && addrinsubnet(peer_client, &sr->that.client)
 	    && (!sr->this.protocol || transport_proto == sr->this.protocol)
 	    && (!sr->this.port || our_port == sr->this.port)
 	    && (!sr->that.port || peer_port == sr->that.port))
	    {
		char cib[CONN_INST_BUF];
		char cib2[CONN_INST_BUF];

 		policy_prio_t prio = 8 * (c->prio + (c->kind == CK_INSTANCE))
 				   + 2 * (sr->this.port == our_port)
 				   + 2 * (sr->that.port == peer_port)
 				   +     (sr->this.protocol == transport_proto);

#ifdef DEBUG
		if (DBGP(DBG_CONTROL|DBG_CONTROLMORE))
		{
		    char c_ocb[SUBNETTOT_BUF], c_pcb[SUBNETTOT_BUF];

		    subnettot(&c->spd.this.client, 0, c_ocb, sizeof(c_ocb));
		    subnettot(&c->spd.that.client, 0, c_pcb, sizeof(c_pcb));
 		    DBG_log("find_connection: "
 			    "conn \"%s\"%s has compatible peers: %s -> %s [pri: %ld]"
			    , c->name
			    , (fmt_conn_instance(c, cib), cib)
			    , c_ocb, c_pcb, prio);
		}
#endif /* DEBUG */

		if (best == NULL)
		{
		    best = c;
		    best_sr = sr;
		    best_prio = prio;
		}

		DBG(DBG_CONTROLMORE,
		    DBG_log("find_connection: comparing best \"%s\"%s [pri:%ld]{%p} (child %s) to \"%s\"%s [pri:%ld]{%p} (child %s)"
			    , best->name
			    , (fmt_conn_instance(best, cib), cib)
			    , best_prio
			    , best
			    , (best->policy_next ? best->policy_next->name : "none")
			    , c->name
			    , (fmt_conn_instance(c, cib2), cib2)
			    , prio
			    , c
			    , (c->policy_next ? c->policy_next->name : "none")));

		if (prio > best_prio)
		{
		    best = c;
		    best_sr = sr;
		    best_prio = prio;
		}
	    }
	}
    }

    if (best!= NULL && NEVER_NEGOTIATE(best->policy))
	best = NULL;

    if (srp != NULL && best != NULL)
	*srp = best_sr;

#ifdef DEBUG
    if (DBGP(DBG_CONTROL))
    {
	if (best)
	{
	    char cib[CONN_INST_BUF];
	    DBG_log("find_connection: concluding with \"%s\"%s [pri:%ld]{%p} kind=%s"
		    , best->name
		    , (fmt_conn_instance(best, cib), cib)
		    , best_prio
		    , best
		    , enum_name(&connection_kind_names, best->kind));
	} else {
	    DBG_log("find_connection: concluding with empty");
	}
    }
#endif /* DEBUG */

    return best;
}

/* Find and instantiate a connection for an outgoing Opportunistic connection.
 * We've already discovered its gateway.
 * We look for a the connection such that:
 *   + this is one of our interfaces
 *   + this subnet contains our_client (or we are our_client)
 *     (we will specialize the client).  We prefer the smallest such subnet.
 *   + that subnet contains peer_clent (we will specialize the client).
 *     We prefer the smallest such subnet.
 *   + is opportunistic
 *   + that peer is NO_IP
 *   + don't care about Phase 1 IDs (probably should be default)
 * We could look for a connection that already had the desired peer
 * (rather than NO_IP) specified, but it doesn't seem worth the
 * bother.
 *
 * We look for the routed policy applying to the narrowest subnets.
 * We only succeed if we find such a policy AND it is satisfactory.
 *
 * The body of the inner loop is a lot like that in
 * find_connection_for_clients.  In this case, we know the gateways
 * that we need to instantiate an opportunistic connection.
 */
struct connection *
build_outgoing_opportunistic_connection(struct gw_info *gw
					,const ip_address *our_client
					,const ip_address *peer_client)
{
    struct iface_port *p;
    struct connection *best = NULL;
    struct spd_route *sr, *bestsr;
    char ocb[ADDRTOT_BUF], pcb[ADDRTOT_BUF];

    addrtot(our_client, 0, ocb, sizeof(ocb));
    addrtot(peer_client, 0, pcb, sizeof(pcb));

    passert(!isanyaddr(our_client) && !isanyaddr(peer_client));

    /* We don't know his ID yet, so gw id must be an ipaddr */
    passert(gw->key != NULL);
    passert(id_is_ipaddr(&gw->gw_id));

    /* for each of our addresses... */
    for (p = interfaces; p != NULL; p = p->next)
    {
	/* go through those connections with our address and NO_IP as hosts
	 * We cannot know what port the peer would use, so we assume
	 * that it is pluto_port (makes debugging easier).
	 */
	struct connection *c = find_host_pair_connections(__FUNCTION__, &p->ip_addr
							  , pluto_port
							  , (ip_address *)NULL
							  , pluto_port);

	for (; c != NULL; c = c->hp_next)
	{
	    DBG(DBG_OPPO,
		DBG_log("checking %s", c->name));
	    if (c->kind == CK_GROUP)
	    {
		continue;
	    }

	    for (sr = &c->spd; best!=c && sr; sr = sr->next)
	    {
		if (routed(sr->routing)
		&& addrinsubnet(our_client, &sr->this.client)
		&& addrinsubnet(peer_client, &sr->that.client))
		{
		    if (best == NULL)
		    {
			best = c;
			break;
		    }

		    DBG(DBG_OPPO,
			DBG_log("comparing best %s to %s"
				, best->name, c->name));

		    for (bestsr = &best->spd; best!=c && bestsr; bestsr=bestsr->next)
		    {
			if (!subnetinsubnet(&bestsr->this.client, &sr->this.client)
			|| (samesubnet(&bestsr->this.client, &sr->this.client)
			     && !subnetinsubnet(&bestsr->that.client
						, &sr->that.client)))
			{
			    best = c;
			}
		    }
		}
	    }
	}
    }

    if (best == NULL
    || NEVER_NEGOTIATE(best->policy)
    || (best->policy & POLICY_OPPO) == LEMPTY
    || best->kind != CK_TEMPLATE)
	return NULL;
    else
	return oppo_instantiate(best, &gw->gw_id.ip_addr, NULL, gw
			      , our_client, peer_client);
}

bool
orient(struct connection *c)
{
    struct spd_route *sr;

    if (!oriented(*c))
    {
	struct iface_port *p;

	for (sr = &c->spd; sr; sr = sr->next)
	{
	    /* Note: this loop does not stop when it finds a match:
	     * it continues checking to catch any ambiguity.
	     */
	    for (p = interfaces; p != NULL; p = p->next)
	    {
#ifdef NAT_TRAVERSAL
		if (p->ike_float) continue;
#endif
		for (;;)
		{
		    /* check if this interface matches this end */
		    if (sameaddr(&sr->this.host_addr, &p->ip_addr)
			&& (kern_interface != NO_KERNEL
			    || sr->this.host_port == pluto_port))
		    {
			if (oriented(*c))
			{
			    if (c->interface->ip_dev == p->ip_dev)
				loglog(RC_LOG_SERIOUS
				       , "both sides of \"%s\" are our interface %s!"
				       , c->name, p->ip_dev->id_rname);
			    else
				loglog(RC_LOG_SERIOUS, "two interfaces match \"%s\" (%s, %s)"
				       , c->name, c->interface->ip_dev->id_rname, p->ip_dev->id_rname);
			    c->interface = NULL;	/* withdraw orientation */
			    return FALSE;
			}
			c->interface = p;
		    }

		    /* done with this interface if it doesn't match that end */
		    if (!(sameaddr(&sr->that.host_addr, &p->ip_addr)
			  && (kern_interface!=NO_KERNEL
			      || sr->that.host_port == pluto_port)))
			break;

		    /* swap ends and try again.
		     * It is a little tricky to see that this loop will stop.
		     * Only continue if the far side matches.
		     * If both sides match, there is an error-out.
		     */
		    {
			struct end t = sr->this;

			sr->this = sr->that;
			sr->that = t;
		    }
		}
	    }
	}
    }
    return oriented(*c);
}

void
initiate_connection(const char *name, int whackfd
		    , lset_t moredebug
		    , enum crypto_importance importance)
{
    struct connection *c = con_by_name(name, TRUE);

    if (c != NULL)
    {
	set_cur_connection(c);

	/* turn on any extra debugging asked for */
	c->extra_debugging |= moredebug;

	if (!oriented(*c))
	{
	    loglog(RC_ORIENT, "We cannot identify ourselves with either end of this connection.");
	}
	else if (NEVER_NEGOTIATE(c->policy))
	{
	    loglog(RC_INITSHUNT
		, "cannot initiate an authby=never connection");
	}
	else if (c->kind != CK_PERMANENT)
	{
	    if (isanyaddr(&c->spd.that.host_addr))
		loglog(RC_NOPEERIP, "cannot initiate connection without knowing peer IP address (kind=%s)"
		       , enum_show(&connection_kind_names, c->kind));
	    else
		loglog(RC_WILDCARD, "cannot initiate connection with ID wildcards (kind=%s)"
		       , enum_show(&connection_kind_names, c->kind));
	}
	else
	{
	    /* We will only request an IPsec SA if policy isn't empty
	     * (ignoring Main Mode items).
	     * This is a fudge, but not yet important.
	     * If we are to proceed asynchronously, whackfd will be NULL_FD.
	     */
	    c->policy |= POLICY_UP;

	    if(c->policy & POLICY_ENCRYPT) {
		struct alg_info_esp *alg = c->alg_info_esp;
		struct db_sa *phase2_sa = kernel_alg_makedb(alg, TRUE);
		
		if(alg != NULL && phase2_sa == NULL) {
		    whack_log(RC_NOALGO, "can not initiate: no acceptable kernel algorithms loaded");
		    reset_cur_connection();
		    close_any(whackfd);
		    return;
		}
		free_sa(phase2_sa);
	    }

#ifdef SMARTCARD
	    /* do we have to prompt for a PIN code? */
	    if (c->spd.this.sc != NULL && !c->spd.this.sc->valid && whackfd != NULL_FD)
		scx_get_pin(c->spd.this.sc, whackfd);

	    if (c->spd.this.sc != NULL && !c->spd.this.sc->valid)
	    {
		loglog(RC_NOVALIDPIN, "cannot initiate connection without valid PIN");
	    }
	    else
#endif
	    {
		ipsecdoi_initiate(whackfd, c, c->policy, 1
				  , SOS_NOBODY, importance);
		whackfd = NULL_FD;	/* protect from close */
	    }
	}
	reset_cur_connection();
    }
    close_any(whackfd);
}

void restart_connections_by_peer(struct connection *c)
{
    struct connection *d;

    d = c->host_pair->connections;
    for (; d != NULL; d = d->hp_next) {
	   if (
#ifdef DYNAMICDNS
	       (c->dnshostname && d->dnshostname && (strcmp(c->dnshostname, d->dnshostname) == 0))
	   	|| (c->dnshostname == NULL && d->dnshostname == NULL && 
#endif /* DYNAMICDNS */
									sameaddr(&d->spd.that.host_addr, &c->spd.that.host_addr)
#ifdef DYNAMICDNS
																)
#endif /* DYNAMICDNS */
																 )
	       terminate_connection(d->name);
    }

#ifdef DYNAMICDNS
    update_host_pairs(c);
#endif /* DYNAMICDNS */

    if (c->host_pair == NULL)
    	   return;
    d = c->host_pair->connections;
    for (; d != NULL; d = d->hp_next) {
    	   if (
#ifdef DYNAMICDNS
	       (c->dnshostname && d->dnshostname && (strcmp(c->dnshostname, d->dnshostname) == 0))
	   	|| (c->dnshostname == NULL && d->dnshostname == NULL && 
#endif /* DYNAMICDNS */
									sameaddr(&d->spd.that.host_addr, &c->spd.that.host_addr)
#ifdef DYNAMICDNS
																)
#endif /* DYNAMICDNS */
																 )
	       initiate_connection(d->name, NULL_FD, 0, pcim_demand_crypto);
    }
}

/* (Possibly) Opportunistic Initiation:
 * Knowing clients (single IP addresses), try to build an tunnel.
 * This may involve discovering a gateway and instantiating an
 * Opportunistic connection.  Called when a packet is caught by
 * a %trap, or when whack --oppohere --oppothere is used.
 * It may turn out that an existing or non-opporunistic connnection
 * can handle the traffic.
 *
 * Most of the code will be restarted if an ADNS request is made
 * to discover the gateway.  The only difference between the first
 * and second entry is whether gateways_from_dns is NULL or not.
 *	initiate_opportunistic: initial entrypoint
 *	continue_oppo: where we pickup when ADNS result arrives
 *	initiate_opportunistic_body: main body shared by above routines
 *	cannot_oppo: a helper function to log a diagnostic
 * This structure repeats a lot of code when the ADNS result arrives.
 * This seems like a waste, but anything learned the first time through
 * may no longer be true!
 *
 * After the first IKE message is sent, the regular state machinery
 * carries negotiation forward.
 */

enum find_oppo_step {
    fos_start,
    fos_myid_ip_txt,
    fos_myid_hostname_txt,
    fos_myid_ip_key,
    fos_myid_hostname_key,
    fos_our_client,
    fos_our_txt,
#ifdef USE_KEYRR
    fos_our_key,
#endif /* USE_KEYRR */
    fos_his_client,
    fos_done
};

#ifdef DEBUG
static const char *const oppo_step_name[] = {
    "fos_start",
    "fos_myid_ip_txt",
    "fos_myid_hostname_txt",
    "fos_myid_ip_key",
    "fos_myid_hostname_key",
    "fos_our_client",
    "fos_our_txt",
#ifdef USE_KEYRR
    "fos_our_key",
#endif /* USE_KEYRR */
    "fos_his_client",
    "fos_done"
};
#endif /* DEBUG */

struct find_oppo_bundle {
    enum find_oppo_step step;
    err_t want;
    bool failure_ok;		/* if true, continue_oppo should not die on DNS failure */
    ip_address our_client;	/* not pointer! */
    ip_address peer_client;
    int transport_proto;
    bool held;
    policy_prio_t policy_prio;
    ipsec_spi_t failure_shunt;	/* in host order!  0 for delete. */
    int whackfd;
};

struct find_oppo_continuation {
    struct adns_continuation ac;	/* common prefix */
    struct find_oppo_bundle b;
};

static void
cannot_oppo(struct connection *c
	    , struct find_oppo_bundle *b
	    , err_t ugh)
{
    char pcb[ADDRTOT_BUF];
    char ocb[ADDRTOT_BUF];

    addrtot(&b->peer_client, 0, pcb, sizeof(pcb));
    addrtot(&b->our_client, 0, ocb, sizeof(ocb));

    openswan_log("Can not opportunistically initiate for %s to %s: %s"
		 , ocb, pcb, ugh);

    whack_log(RC_OPPOFAILURE
	, "Can not opportunistically initiate for %s to %s: %s"
	, ocb, pcb, ugh);

    if (c != NULL && c->policy_next != NULL)
    {
	/* there is some policy that comes afterwards */
	struct spd_route *shunt_spd;
	struct connection *nc = c->policy_next;
	struct state *st;

	passert(c->kind == CK_TEMPLATE);
	passert(c->policy_next->kind == CK_PERMANENT);

	DBG(DBG_OPPO, DBG_log("OE failed for %s to %s, but %s overrides shunt"
			      , ocb, pcb, c->policy_next->name));

	/*
	 * okay, here we need add to the "next" policy, which is ought
	 * to be an instance.
	 * We will add another entry to the spd_route list for the specific
	 * situation that we have.
	 */

	shunt_spd = clone_thing(nc->spd, "shunt eroute policy");

	shunt_spd->next = nc->spd.next;
	nc->spd.next = shunt_spd;

	happy(addrtosubnet(&b->peer_client, &shunt_spd->that.client));

	if (sameaddr(&b->peer_client, &shunt_spd->that.host_addr))
	    shunt_spd->that.has_client = FALSE;

	/*
	 * override the tunnel destination with the one from the secondaried
	 * policy
	 */
	shunt_spd->that.host_addr = nc->spd.that.host_addr;

	/* now, lookup the state, and poke it up.
	 */

	st = state_with_serialno(nc->newest_ipsec_sa);

	/* XXX what to do if the IPSEC SA has died? */
	passert(st != NULL);

	/* link the new connection instance to the state's list of
	 * connections
	 */

	DBG(DBG_OPPO, DBG_log("installing state: %ld for %s to %s"
			      , nc->newest_ipsec_sa
			      , ocb, pcb));

#ifdef DEBUG
	if (DBGP(DBG_OPPO | DBG_CONTROLMORE))
	{
	    char state_buf[LOG_WIDTH];
	    char state_buf2[LOG_WIDTH];
	    time_t n = now();

	    fmt_state(st, n, state_buf, sizeof(state_buf)
		      , state_buf2, sizeof(state_buf2));
	    DBG_log("cannot_oppo, failure SA1: %s", state_buf);
	    DBG_log("cannot_oppo, failure SA2: %s", state_buf2);
	}
#endif /* DEBUG */

	if (!route_and_eroute(c, shunt_spd, st))
	{
	    whack_log(RC_OPPOFAILURE
		      , "failed to instantiate shunt policy %s for %s to %s"
		      , c->name
		      , ocb, pcb);
	}
	return;
    }

#ifdef KLIPS
    if (b->held)
    {
	/* Replace HOLD with b->failure_shunt.
	 * If no b->failure_shunt specified, use SPI_PASS -- THIS MAY CHANGE.
	 */
	if (b->failure_shunt == 0)
	{
	    DBG(DBG_OPPO, DBG_log("no explicit failure shunt for %s to %s; installing %%pass"
				  , ocb, pcb));
	}

	(void) replace_bare_shunt(&b->our_client, &b->peer_client
	    , b->policy_prio
	    , b->failure_shunt
	    , b->failure_shunt != 0
	    , b->transport_proto
	    , ugh);
    }
#endif
}

static void initiate_opportunistic_body(struct find_oppo_bundle *b
    , struct adns_continuation *ac, err_t ac_ugh);	/* forward */

void
initiate_opportunistic(const ip_address *our_client
, const ip_address *peer_client
, int transport_proto UNUSED
, bool held
, int whackfd
, err_t why)
{
    struct find_oppo_bundle b;

    b.want = why;	/* fudge */
    b.failure_ok = FALSE;
    b.our_client = *our_client;
    b.peer_client = *peer_client;
    b.transport_proto = 0;
    b.held = held;
    b.policy_prio = BOTTOM_PRIO;
    b.failure_shunt = 0;
    b.whackfd = whackfd;
    b.step = fos_start;
    initiate_opportunistic_body(&b, NULL, NULL);
}

static void
continue_oppo(struct adns_continuation *acr, err_t ugh)
{
    struct find_oppo_continuation *cr = (void *)acr;	/* inherit, damn you! */
    struct connection *c;
    bool was_held = cr->b.held;
    int whackfd = cr->b.whackfd;

    /* note: cr->id has no resources; cr->sgw_id is id_none:
     * neither need freeing.
     */
    whack_log_fd = whackfd;

#ifdef KLIPS
    /* Discover and record whether %hold has gone away.
     * This could have happened while we were awaiting DNS.
     * We must check BEFORE any call to cannot_oppo.
     */
    if (was_held)
	cr->b.held = has_bare_hold(&cr->b.our_client, &cr->b.peer_client
	    , cr->b.transport_proto);
#endif

#ifdef DEBUG
    /* if we're going to ignore the error, at least note it in debugging log */
    if (cr->b.failure_ok && ugh != NULL)
    {
	DBG(DBG_CONTROL | DBG_DNS,
	    {
		char ocb[ADDRTOT_BUF];
		char pcb[ADDRTOT_BUF];

		addrtot(&cr->b.our_client, 0, ocb, sizeof(ocb));
		addrtot(&cr->b.peer_client, 0, pcb, sizeof(pcb));
		DBG_log("continuing from failed DNS lookup for %s, %s to %s: %s"
		    , cr->b.want, ocb, pcb, ugh);
	    });
    }
#endif

    if (!cr->b.failure_ok && ugh != NULL)
    {
	c = find_connection_for_clients(NULL, &cr->b.our_client, &cr->b.peer_client
	    , cr->b.transport_proto);
	cannot_oppo(c, &cr->b
		    , builddiag("%s: %s", cr->b.want, ugh));
    }
    else if (was_held && !cr->b.held)
    {
	/* was_held indicates we were started due to a %trap firing
	 * (as opposed to a "whack --oppohere --oppothere").
	 * Since the %hold has gone, we can assume that somebody else
	 * has beaten us to the punch.  We can go home.  But lets log it.
	 */
	char ocb[ADDRTOT_BUF];
	char pcb[ADDRTOT_BUF];

	addrtot(&cr->b.our_client, 0, ocb, sizeof(ocb));
	addrtot(&cr->b.peer_client, 0, pcb, sizeof(pcb));

	loglog(RC_COMMENT
	    , "%%hold otherwise handled during DNS lookup for Opportunistic Initiation for %s to %s"
	    , ocb, pcb);
    }
    else
    {
	initiate_opportunistic_body(&cr->b, &cr->ac, ugh);
	whackfd = NULL_FD;	/* was handed off */
    }

    whack_log_fd = NULL_FD;
    close_any(whackfd);
}

#ifdef USE_KEYRR
static err_t
check_key_recs(enum myid_state try_state
, const struct connection *c
, struct adns_continuation *ac)
{
    /* Check if KEY lookup yielded good results.
     * Looking up based on our ID.  Used if
     * client is ourself, or if TXT had no public key.
     * Note: if c is different this time, there is
     * a chance that we did the wrong query.
     * If so, treat as a kind of failure.
     */
    enum myid_state old_myid_state = myid_state;
    const struct RSA_private_key *our_RSA_pri;
    err_t ugh = NULL;

    myid_state = try_state;

    if (old_myid_state != myid_state
    && old_myid_state == MYID_SPECIFIED)
    {
	ugh = "%myid was specified while we were guessing";
    }
    else if ((our_RSA_pri = get_RSA_private_key(c)) == NULL)
    {
	ugh = "we don't know our own RSA key";
    }
    else if (!same_id(&ac->id, &c->spd.this.id))
    {
	ugh = "our ID changed underfoot";
    }
    else
    {
	/* Similar to code in RSA_check_signature
	 * for checking the other side.
	 */
	struct pubkey_list *kr;

	ugh = "no KEY RR found for us";
	for (kr = ac->keys_from_dns; kr != NULL; kr = kr->next)
	{
	    ugh = "all our KEY RRs have the wrong public key";
	    if (kr->key->alg == PUBKEY_ALG_RSA
	    && same_RSA_public_key(&our_RSA_pri->pub, &kr->key->u.rsa))
	    {
		ugh = NULL;	/* good! */
		break;
	    }
	}
    }
    if (ugh != NULL)
	myid_state = old_myid_state;
    return ugh;
}
#endif /* USE_KEYRR */

static err_t
check_txt_recs(enum myid_state try_state
, const struct connection *c
, struct adns_continuation *ac)
{
    /* Check if TXT lookup yielded good results.
     * Looking up based on our ID.  Used if
     * client is ourself, or if TXT had no public key.
     * Note: if c is different this time, there is
     * a chance that we did the wrong query.
     * If so, treat as a kind of failure.
     */
    enum myid_state old_myid_state = myid_state;
    const struct RSA_private_key *our_RSA_pri;
    err_t ugh = NULL;

    myid_state = try_state;

    if (old_myid_state != myid_state
    && old_myid_state == MYID_SPECIFIED)
    {
	ugh = "%myid was specified while we were guessing";
    }
    else if ((our_RSA_pri = get_RSA_private_key(c)) == NULL)
    {
	ugh = "we don't know our own RSA key";
    }
    else if (!same_id(&ac->id, &c->spd.this.id))
    {
	ugh = "our ID changed underfoot";
    }
    else
    {
	/* Similar to code in RSA_check_signature
	 * for checking the other side.
	 */
	struct gw_info *gwp;

	ugh = "no TXT RR found for us";
	for (gwp = ac->gateways_from_dns; gwp != NULL; gwp = gwp->next)
	{
	    ugh = "all our TXT RRs have the wrong public key";
	    if (gwp->key->alg == PUBKEY_ALG_RSA
	    && same_RSA_public_key(&our_RSA_pri->pub, &gwp->key->u.rsa))
	    {
		ugh = NULL;	/* good! */
		break;
	    }
	}
    }
    if (ugh != NULL)
	myid_state = old_myid_state;
    return ugh;
}


/* note: gateways_from_dns must be NULL iff this is the first call */
static void
initiate_opportunistic_body(struct find_oppo_bundle *b
, struct adns_continuation *ac
, err_t ac_ugh)
{
    struct connection *c;
    struct spd_route *sr;
    char ours[ADDRTOT_BUF];
    char his[ADDRTOT_BUF];
    int ourport;
    int hisport;
    char demandbuf[256];
    bool loggedit = FALSE;

    
    /* What connection shall we use?
     * First try for one that explicitly handles the clients.
     */
    
    addrtot(&b->our_client, 0, ours, sizeof(ours));
    addrtot(&b->peer_client, 0, his, sizeof(his));
    ourport = ntohs(portof(&b->our_client));
    hisport = ntohs(portof(&b->peer_client));

    snprintf(demandbuf, 256, "initiate on demand from %s:%d to %s:%d proto=%d state: %s because: %s"
	     , ours, ourport, his, hisport, b->transport_proto
	     , oppo_step_name[b->step], b->want);
    
    if(DBGP(DBG_OPPO)) {
	DBG_log("%s", demandbuf);
	loggedit = TRUE;
    } else if(whack_log_fd != NULL_FD) {
	whack_log(RC_COMMENT, "%s", demandbuf);
	loggedit = TRUE;
    }

    if (isanyaddr(&b->our_client) || isanyaddr(&b->peer_client))
    {
	cannot_oppo(NULL, b, "impossible IP address");
    }
    else if ((c = find_connection_for_clients(&sr
					      , &b->our_client
					      , &b->peer_client
					      , b->transport_proto)) == NULL)
    {
	/* No connection explicitly handles the clients and there
	 * are no Opportunistic connections -- whine and give up.
	 * The failure policy cannot be gotten from a connection; we pick %pass.
	 */
	if(!loggedit) { openswan_log("%s", demandbuf); loggedit=TRUE; }
	cannot_oppo(NULL, b, "no routed template covers this pair");
    }
    else if (c->kind == CK_TEMPLATE && (c->policy & POLICY_OPPO)==0)
    {
	if(!loggedit) { openswan_log("%s", demandbuf); loggedit=TRUE; }
	loglog(RC_NOPEERIP, "cannot initiate connection for packet %s:%d -> %s:%d proto=%d - template conn"
	       , ours, ourport, his, hisport, b->transport_proto);
    }
    else if (c->kind != CK_TEMPLATE)
    {
	/* We've found a connection that can serve.
	 * Do we have to initiate it?
	 * Not if there is currently an IPSEC SA.
	 * But if there is an IPSEC SA, then KLIPS would not
	 * have generated the acquire.  So we assume that there isn't one.
	 * This may be redundant if a non-opportunistic
	 * negotiation is already being attempted.
	 */

	/* If we are to proceed asynchronously, b->whackfd will be NULL_FD. */

	if(c->kind == CK_INSTANCE)
	{
	    char cib[CONN_INST_BUF];
	    /* there is already an instance being negotiated, do nothing */
	    openswan_log("rekeing existing instance \"%s\"%s, due to acquire"
			 , c->name
			 , (fmt_conn_instance(c, cib), cib));

	    /*
	     * we used to return here, but rekeying is a better choice. If we
	     * got the acquire, it is because something turned stuff into a
	     * %trap, or something got deleted, perhaps due to an expiry.
	     */
	}

	/* otherwise, there is some kind of static conn that can handle
	 * this connection, so we initiate it */

#ifdef KLIPS
	if (b->held)
	{
	    /* what should we do on failure? */
	    (void) assign_hold(c, sr, b->transport_proto, &b->our_client, &b->peer_client);
	}
#endif

	if(!loggedit) { openswan_log("%s", demandbuf); loggedit=TRUE; }
	ipsecdoi_initiate(b->whackfd, c, c->policy, 1
			  , SOS_NOBODY, pcim_local_crypto);
	b->whackfd = NULL_FD;	/* protect from close */
    }
    else
    {
	/* We are handling an opportunistic situation.
	 * This involves several DNS lookup steps that require suspension.
	 * Note: many facts might change while we're suspended.
	 * Here be dragons.
	 *
	 * The first chunk of code handles the result of the previous
	 * DNS query (if any).  It also selects the kind of the next step.
	 * The second chunk initiates the next DNS query (if any).
	 */
	enum find_oppo_step next_step;
	err_t ugh = ac_ugh;
	char mycredentialstr[IDTOA_BUF];
	char cib[CONN_INST_BUF];

	DBG(DBG_CONTROL, DBG_log("creating new instance from \"%s\"%s"
				 , c->name
				 , (fmt_conn_instance(c, cib), cib)));

	idtoa(&sr->this.id, mycredentialstr, sizeof(mycredentialstr));

	passert(c->policy & POLICY_OPPO);   /* can't initiate Road Warrior connections */

	/* handle any DNS answer; select next step */

	switch (b->step)
	{
	case fos_start:
	    /* just starting out: select first query step */
	    next_step = fos_myid_ip_txt;
	    break;

	case fos_myid_ip_txt:	/* TXT for our default IP address as %myid */
	    ugh = check_txt_recs(MYID_IP, c, ac);
	    if (ugh != NULL)
	    {
		/* cannot use our IP as OE identitiy for initiation */
		DBG(DBG_OPPO, DBG_log("can not use our IP (%s:TXT) as identity: %s"
				      , myid_str[MYID_IP]
				      , ugh));
		if (!logged_myid_ip_txt_warning)
		{
		    loglog(RC_LOG_SERIOUS
			   , "can not use our IP (%s:TXT) as identity: %s"
			   , myid_str[MYID_IP]
			   , ugh);
		    logged_myid_ip_txt_warning = TRUE;
		}

		next_step = fos_myid_hostname_txt;
		ugh = NULL;	/* failure can be recovered from */
	    }
	    else
	    {
		/* we can use our IP as OE identity for initiation */
		if (!logged_myid_ip_txt_warning)
		{
		    loglog(RC_LOG_SERIOUS
			   , "using our IP (%s:TXT) as identity!"
			   , myid_str[MYID_IP]);
		    logged_myid_ip_txt_warning = TRUE;
		}

		next_step = fos_our_client;
	    }
	    break;

	case fos_myid_hostname_txt:	/* TXT for our hostname as %myid */
	    ugh = check_txt_recs(MYID_HOSTNAME, c, ac);
	    if (ugh != NULL)
	    {
		/* cannot use our hostname as OE identitiy for initiation */
		DBG(DBG_OPPO, DBG_log("can not use our hostname (%s:TXT) as identity: %s"
				      , myid_str[MYID_HOSTNAME]
				      , ugh));
		if (!logged_myid_fqdn_txt_warning)
		{
		    loglog(RC_LOG_SERIOUS
			   , "can not use our hostname (%s:TXT) as identity: %s"
			   , myid_str[MYID_HOSTNAME]
			   , ugh);
		    logged_myid_fqdn_txt_warning = TRUE;
		}
#ifdef USE_KEYRR
		next_step = fos_myid_ip_key;
		ugh = NULL;	/* failure can be recovered from */
#endif
	    }
	    else
	    {
		/* we can use our hostname as OE identity for initiation */
		if (!logged_myid_fqdn_txt_warning)
		{
		    loglog(RC_LOG_SERIOUS
			   , "using our hostname (%s:TXT) as identity!"
			   , myid_str[MYID_HOSTNAME]);
		    logged_myid_fqdn_txt_warning = TRUE;
		}
		next_step = fos_our_client;
	    }
	    break;

#ifdef USE_KEYRR
	case fos_myid_ip_key:	/* KEY for our default IP address as %myid */
	    ugh = check_key_recs(MYID_IP, c, ac);
	    if (ugh != NULL)
	    {
		/* cannot use our IP as OE identitiy for initiation */
		DBG(DBG_OPPO, DBG_log("can not use our IP (%s:KEY) as identity: %s"
				      , myid_str[MYID_IP]
				      , ugh));
		if (!logged_myid_ip_key_warning)
		{
		    loglog(RC_LOG_SERIOUS
			   , "can not use our IP (%s:KEY) as identity: %s"
			   , myid_str[MYID_IP]
			   , ugh);
		    logged_myid_ip_key_warning = TRUE;
		}

		next_step = fos_myid_hostname_key;
		ugh = NULL;	/* failure can be recovered from */
	    }
	    else
	    {
		/* we can use our IP as OE identity for initiation */
		if (!logged_myid_ip_key_warning)
		{
		    loglog(RC_LOG_SERIOUS
			   , "using our IP (%s:KEY) as identity!"
			   , myid_str[MYID_IP]);
		    logged_myid_ip_key_warning = TRUE;
		}
		next_step = fos_our_client;
	    }
	    break;

	case fos_myid_hostname_key:	/* KEY for our hostname as %myid */
	    ugh = check_key_recs(MYID_HOSTNAME, c, ac);
	    if (ugh != NULL)
	    {
		/* cannot use our IP as OE identitiy for initiation */
		DBG(DBG_OPPO, DBG_log("can not use our hostname (%s:KEY) as identity: %s"
				      , myid_str[MYID_HOSTNAME]
				      , ugh));
		if (!logged_myid_fqdn_key_warning)
		{
		    loglog(RC_LOG_SERIOUS
			   , "can not use our hostname (%s:KEY) as identity: %s"
			   , myid_str[MYID_HOSTNAME]
			   , ugh);
		    logged_myid_fqdn_key_warning = TRUE;
		}

		next_step = fos_myid_hostname_key;
		ugh = NULL;	/* failure can be recovered from */
	    }
	    else
	    {
		/* we can use our IP as OE identity for initiation */
		if (!logged_myid_fqdn_key_warning)
		{
		    loglog(RC_LOG_SERIOUS
			   , "using our hostname (%s:KEY) as identity!"
			   , myid_str[MYID_HOSTNAME]);
		    logged_myid_fqdn_key_warning = TRUE;
		}
		next_step = fos_our_client;
	    }
	    break;
#endif

	case fos_our_client:	/* TXT for our client */
	    {
		/* Our client is not us: we must check the TXT records.
		 * Note: if c is different this time, there is
		 * a chance that we did the wrong query.
		 * If so, treat as a kind of failure.
		 */
		const struct RSA_private_key *our_RSA_pri = get_RSA_private_key(c);

		next_step = fos_his_client;	/* normal situation */

		passert(sr != NULL);

		if (our_RSA_pri == NULL)
		{
		    ugh = "we don't know our own RSA key";
		}
		else if (sameaddr(&sr->this.host_addr, &b->our_client))
		{
		    /* this wasn't true when we started -- bail */
		    ugh = "our IP address changed underfoot";
		}
		else if (!same_id(&ac->sgw_id, &sr->this.id))
		{
		    /* this wasn't true when we started -- bail */
		    ugh = "our ID changed underfoot";
		}
		else
		{
		    /* Similar to code in quick_inI1_outR1_tail
		     * for checking the other side.
		     */
		    struct gw_info *gwp;

		    ugh = "no TXT RR for our client delegates us";
		    for (gwp = ac->gateways_from_dns; gwp != NULL; gwp = gwp->next)
		    {
			passert(same_id(&gwp->gw_id, &sr->this.id));

			ugh = "TXT RR for our client has wrong key";
			/* If there is a key from the TXT record,
			 * we count it as a win if we match the key.
			 * If there was no key, we have a tentative win:
			 * we need to check our KEY record to be sure.
			 */
			if (!gwp->gw_key_present)
			{
			    /* Success, but the TXT had no key
			     * so we must check our our own KEY records.
			     */
			    next_step = fos_our_txt;
			    ugh = NULL;	/* good! */
			    break;
			}
			if (same_RSA_public_key(&our_RSA_pri->pub, &gwp->key->u.rsa))
			{
			    ugh = NULL;	/* good! */
			    break;
			}
		    }
		}
	    }
	    break;

	case fos_our_txt:	/* TXT for us */
	    {
		/* Check if TXT lookup yielded good results.
		 * Looking up based on our ID.  Used if
		 * client is ourself, or if TXT had no public key.
		 * Note: if c is different this time, there is
		 * a chance that we did the wrong query.
		 * If so, treat as a kind of failure.
		 */
		const struct RSA_private_key *our_RSA_pri = get_RSA_private_key(c);

		next_step = fos_his_client;	/* unless we decide to look for KEY RR */

		if (our_RSA_pri == NULL)
		{
		    ugh = "we don't know our own RSA key";
		}
		else if (!same_id(&ac->id, &c->spd.this.id))
		{
		    ugh = "our ID changed underfoot";
		}
		else
		{
		    /* Similar to code in RSA_check_signature
		     * for checking the other side.
		     */
		    struct gw_info *gwp;

		    ugh = "no TXT RR for us";
		    for (gwp = ac->gateways_from_dns; gwp != NULL; gwp = gwp->next)
		    {
			passert(same_id(&gwp->gw_id, &sr->this.id));

			ugh = "TXT RR for us has wrong key";
			if (gwp->gw_key_present
			&& same_RSA_public_key(&our_RSA_pri->pub, &gwp->key->u.rsa))
			{
			    DBG(DBG_CONTROL,
				DBG_log("initiate on demand found TXT with right public key at: %s"
					, mycredentialstr));
			    ugh = NULL;
			    break;
			}
		    }
#ifdef USE_KEYRR
		    if (ugh != NULL)
		    {
			/* if no TXT with right key, try KEY */
			DBG(DBG_CONTROL,
			    DBG_log("will try for KEY RR since initiate on demand found %s: %s"
				    , ugh, mycredentialstr));
			next_step = fos_our_key;
			ugh = NULL;
		    }
#endif
		}
	    }
	    break;

#ifdef USE_KEYRR
	case fos_our_key:	/* KEY for us */
	    {
		/* Check if KEY lookup yielded good results.
		 * Looking up based on our ID.  Used if
		 * client is ourself, or if TXT had no public key.
		 * Note: if c is different this time, there is
		 * a chance that we did the wrong query.
		 * If so, treat as a kind of failure.
		 */
		const struct RSA_private_key *our_RSA_pri = get_RSA_private_key(c);

		next_step = fos_his_client;	/* always */

		if (our_RSA_pri == NULL)
		{
		    ugh = "we don't know our own RSA key";
		}
		else if (!same_id(&ac->id, &c->spd.this.id))
		{
		    ugh = "our ID changed underfoot";
		}
		else
		{
		    /* Similar to code in RSA_check_signature
		     * for checking the other side.
		     */
		    struct pubkey_list *kr;

		    ugh = "no KEY RR found for us (and no good TXT RR)";
		    for (kr = ac->keys_from_dns; kr != NULL; kr = kr->next)
		    {
			ugh = "all our KEY RRs have the wrong public key (and no good TXT RR)";
			if (kr->key->alg == PUBKEY_ALG_RSA
			&& same_RSA_public_key(&our_RSA_pri->pub, &kr->key->u.rsa))
			{
			    /* do this only once a day */
			    if (!logged_txt_warning)
			    {
				loglog(RC_LOG_SERIOUS
				       , "found KEY RR but not TXT RR for %s."
				       , mycredentialstr);
				logged_txt_warning = TRUE;
			    }
			    ugh = NULL;	/* good! */
			    break;
			}
		    }
		}
	    }
	    break;
#endif /* USE_KEYRR */

	case fos_his_client:	/* TXT for his client */
	    {
		/* We've finished last DNS queries: TXT for his client.
		 * Using the information, try to instantiate a connection
		 * and start negotiating.
		 * We now know the peer.  The chosing of "c" ignored this,
		 * so we will disregard its current value.
		 * !!! We need to randomize the entry in gw that we choose.
		 */
		next_step = fos_done;	/* no more queries */

		c = build_outgoing_opportunistic_connection(ac->gateways_from_dns
							    , &b->our_client
							    , &b->peer_client);

		if (c == NULL)
		{
		    /* We cannot seem to instantiate a suitable connection:
		     * complain clearly.
		     */
		    char ocb[ADDRTOT_BUF]
			, pcb[ADDRTOT_BUF]
			, pb[ADDRTOT_BUF];

		    addrtot(&b->our_client, 0, ocb, sizeof(ocb));
		    addrtot(&b->peer_client, 0, pcb, sizeof(pcb));
		    passert(id_is_ipaddr(&ac->gateways_from_dns->gw_id));
		    addrtot(&ac->gateways_from_dns->gw_id.ip_addr, 0, pb, sizeof(pb));
		    loglog(RC_OPPOFAILURE
			, "no suitable connection for opportunism"
			  " between %s and %s with %s as peer"
			, ocb, pcb, pb);

#ifdef KLIPS
		    if (b->held)
		    {
			/* Replace HOLD with PASS.
			 * The type of replacement *ought* to be
			 * specified by policy.
			 */
			(void) replace_bare_shunt(&b->our_client, &b->peer_client
			    , BOTTOM_PRIO
			    , SPI_PASS	/* fail into PASS */
			    , TRUE, b->transport_proto
			    , "no suitable connection");
		    }
#endif
		}
		else
		{
		    /* If we are to proceed asynchronously, b->whackfd will be NULL_FD. */
		    passert(c->kind == CK_INSTANCE);
		    passert(c->gw_info != NULL);
		    passert(HAS_IPSEC_POLICY(c->policy));
		    passert(LHAS(LELEM(RT_UNROUTED) | LELEM(RT_ROUTED_PROSPECTIVE), c->spd.routing));
#ifdef KLIPS
		    if (b->held)
		    {
			/* what should we do on failure? */
			(void) assign_hold(c, &c->spd
					   , b->transport_proto
					   , &b->our_client, &b->peer_client);
		    }
#endif
		    c->gw_info->key->last_tried_time = now();
		    DBG(DBG_CONTROL,
			DBG_log("initiate on demand from %s:%d to %s:%d proto=%d state: %s because: %s"
				 , ours, ourport, his, hisport, b->transport_proto
				 , oppo_step_name[b->step], b->want));

		    ipsecdoi_initiate(b->whackfd, c, c->policy, 1
				      , SOS_NOBODY, pcim_local_crypto);
		    b->whackfd = NULL_FD;	/* protect from close */
		}
	    }
	    break;

	default:
	    bad_case(b->step);
	}

	/* the second chunk: initiate the next DNS query (if any) */
	DBG(DBG_CONTROL,
	{
	    char ours[ADDRTOT_BUF];
	    char his[ADDRTOT_BUF];

	    addrtot(&b->our_client, 0, ours, sizeof(ours));
	    addrtot(&b->peer_client, 0, his, sizeof(his));
	    DBG_log("initiate on demand from %s to %s new state: %s with ugh: %s"
		    , ours, his, oppo_step_name[b->step], ugh ? ugh : "ok");
	});

	if (ugh != NULL)
	{
	    b->policy_prio = c->prio;
	    b->failure_shunt = shunt_policy_spi(c, FALSE);
	    cannot_oppo(c, b, ugh);
	}
	else if (next_step == fos_done)
	{
	    /* nothing to do */
	}
	else
	{
	    /* set up the next query */
	    struct find_oppo_continuation *cr = alloc_thing(struct find_oppo_continuation
		, "opportunistic continuation");
	    struct id id;

	    b->policy_prio = c->prio;
	    b->failure_shunt = shunt_policy_spi(c, FALSE);
	    cr->b = *b;	/* copy; start hand off of whackfd */
	    cr->b.failure_ok = FALSE;
	    cr->b.step = next_step;

	    for (sr = &c->spd
	    ; sr!=NULL && !sameaddr(&sr->this.host_addr, &b->our_client)
	    ; sr = sr->next)
		;

	    if (sr == NULL)
		sr = &c->spd;

	    /* If a %hold shunt has replaced the eroute for this template,
	     * record this fact.
	     */
	    if (b->held
	    && sr->routing == RT_ROUTED_PROSPECTIVE && eclipsable(sr))
	    {
		sr->routing = RT_ROUTED_ECLIPSED;
		eclipse_count++;
	    }

	    /* Switch to issue next query.
	     * A case may turn out to be unnecessary.  If so, it falls
	     * through to the next case.
	     * Figuring out what %myid can stand for must be done before
	     * our client credentials are looked up: we must know what
	     * the client credentials may use to identify us.
	     * On the other hand, our own credentials should be looked
	     * up after our clients in case our credentials are not
	     * needed at all.
	     * XXX this is a wasted effort if we don't have credentials
	     * BUT they are not needed.
	     */
	    switch (next_step)
	    {
	    case fos_myid_ip_txt:
		if (c->spd.this.id.kind == ID_MYID
		&& myid_state != MYID_SPECIFIED)
		{
		    cr->b.failure_ok = TRUE;
		    cr->b.want = b->want = "TXT record for IP address as %myid";
		    ugh = start_adns_query(&myids[MYID_IP]
			, &myids[MYID_IP]
			, ns_t_txt
			, continue_oppo
			, &cr->ac);
		    break;
		}
		cr->b.step = fos_myid_hostname_txt;
		/* fall through */

	    case fos_myid_hostname_txt:
		if (c->spd.this.id.kind == ID_MYID
		&& myid_state != MYID_SPECIFIED)
		{
#ifdef USE_KEYRR
		    cr->b.failure_ok = TRUE;
#else
		    cr->b.failure_ok = FALSE;
#endif
		    cr->b.want = b->want = "TXT record for hostname as %myid";
		    ugh = start_adns_query(&myids[MYID_HOSTNAME]
			, &myids[MYID_HOSTNAME]
			, ns_t_txt
			, continue_oppo
			, &cr->ac);
		    break;
		}

#ifdef USE_KEYRR
		cr->b.step = fos_myid_ip_key;
		/* fall through */

	    case fos_myid_ip_key:
		if (c->spd.this.id.kind == ID_MYID
		&& myid_state != MYID_SPECIFIED)
		{
		    cr->b.failure_ok = TRUE;
		    cr->b.want = b->want = "KEY record for IP address as %myid (no good TXT)";
		    ugh = start_adns_query(&myids[MYID_IP]
			, (const struct id *) NULL	/* security gateway meaningless */
			, ns_t_key
			, continue_oppo
			, &cr->ac);
		    break;
		}
		cr->b.step = fos_myid_hostname_key;
		/* fall through */

	    case fos_myid_hostname_key:
		if (c->spd.this.id.kind == ID_MYID
		&& myid_state != MYID_SPECIFIED)
		{
		    cr->b.failure_ok = FALSE;		/* last attempt! */
		    cr->b.want = b->want = "KEY record for hostname as %myid (no good TXT)";
		    ugh = start_adns_query(&myids[MYID_HOSTNAME]
			, (const struct id *) NULL	/* security gateway meaningless */
			, ns_t_key
			, continue_oppo
			, &cr->ac);
		    break;
		}
#endif
		cr->b.step = fos_our_client;
		/* fall through */

	    case fos_our_client:	/* TXT for our client */
		if (!sameaddr(&c->spd.this.host_addr, &b->our_client))
		{
		    /* Check that at least one TXT(reverse(b->our_client)) is workable.
		     * Note: {unshare|free}_id_content not needed for id: ephemeral.
		     */
		    cr->b.want = b->want = "our client's TXT record";
		    iptoid(&b->our_client, &id);
		    ugh = start_adns_query(&id
			, &c->spd.this.id	/* we are the security gateway */
			, ns_t_txt
			, continue_oppo
			, &cr->ac);
		    break;
		}
		cr->b.step = fos_our_txt;
		/* fall through */

	    case fos_our_txt:	/* TXT for us */
		cr->b.failure_ok = b->failure_ok = TRUE;
		cr->b.want = b->want = "our TXT record";
		ugh = start_adns_query(&sr->this.id
		    , &sr->this.id	/* we are the security gateway XXX - maybe ignore? mcr */
		    , ns_t_txt
		    , continue_oppo
		    , &cr->ac);
		break;

#ifdef USE_KEYRR
	    case fos_our_key:	/* KEY for us */
		cr->b.want = b->want = "our KEY record";
		cr->b.failure_ok = b->failure_ok = FALSE;
		ugh = start_adns_query(&sr->this.id
		    , (const struct id *) NULL	/* security gateway meaningless */
		    , ns_t_key
		    , continue_oppo
		    , &cr->ac);
		break;
#endif /* USE_KEYRR */

	    case fos_his_client:	/* TXT for his client */
		/* note: {unshare|free}_id_content not needed for id: ephemeral */
		cr->b.want = b->want = "target's TXT record";
		cr->b.failure_ok = b->failure_ok = FALSE;
		iptoid(&b->peer_client, &id);
		ugh = start_adns_query(&id
		    , (const struct id *) NULL	/* security gateway unconstrained */
		    , ns_t_txt
		    , continue_oppo
		    , &cr->ac);
		break;

	    default:
		bad_case(next_step);
	    }

	    if (ugh == NULL)
		b->whackfd = NULL_FD;	/* complete hand-off */
	    else
		cannot_oppo(c, b, ugh);
	}
    }
    close_any(b->whackfd);
}

void
terminate_connection(const char *nm)
{
    /* Loop because more than one may match (master and instances)
     * But at least one is required (enforced by con_by_name).
     */
    struct connection *c, *n;

    for (c = con_by_name(nm, TRUE); c != NULL; c = n)
    {
	n = c->ac_next;	/* grab this before c might disappear */
	if (streq(c->name, nm)
	&& c->kind >= CK_PERMANENT
	&& !NEVER_NEGOTIATE(c->policy))
	{
	    set_cur_connection(c);
	    openswan_log("terminating SAs using this connection");
	    c->policy &= ~POLICY_UP;
	    flush_pending_by_connection(c);
	    delete_states_by_connection(c, FALSE);
	    reset_cur_connection();
	}
    }
}

/* check nexthop safety
 * Our nexthop must not be within a routed client subnet, and vice versa.
 * Note: we don't think this is true.  We think that KLIPS will
 * not process a packet output by an eroute.
 */
#ifdef NEVER
//bool
//check_nexthop(const struct connection *c)
//{
//    struct connection *d;
//
//    if (addrinsubnet(&c->spd.this.host_nexthop, &c->spd.that.client))
//    {
//	loglog(RC_LOG_SERIOUS, "cannot perform routing for connection \"%s\""
//	    " because nexthop is within peer's client network",
//	    c->name);
//	return FALSE;
//    }
//
//    for (d = connections; d != NULL; d = d->next)
//    {
//	if (d->routing != RT_UNROUTED)
//	{
//	    if (addrinsubnet(&c->spd.this.host_nexthop, &d->spd.that.client))
//	    {
//		loglog(RC_LOG_SERIOUS, "cannot do routing for connection \"%s\"
//		    " because nexthop is contained in"
//		    " existing routing for connection \"%s\"",
//		    c->name, d->name);
//		return FALSE;
//	    }
//	    if (addrinsubnet(&d->spd.this.host_nexthop, &c->spd.that.client))
//	    {
//		loglog(RC_LOG_SERIOUS, "cannot do routing for connection \"%s\"
//		    " because it contains nexthop of"
//		    " existing routing for connection \"%s\"",
//		    c->name, d->name);
//		return FALSE;
//	    }
//	}
//    }
//    return TRUE;
//}
#endif /* NEVER */

/* an ISAKMP SA has been established.
 * Note the serial number, and release any connections with
 * the same peer ID but different peer IP address.
 */
bool uniqueIDs = FALSE;	/* --uniqueids? */

void
ISAKMP_SA_established(struct connection *c, so_serial_t serial)
{
    c->newest_isakmp_sa = serial;

    if (uniqueIDs
#ifdef XAUTH
	 && (!c->spd.this.xauth_server)
#endif
	)
{
	/* for all connections: if the same Phase 1 peer ID is used
	 * for a different IP address, unorient that connection.
	 */
	struct connection *d;

	for (d = connections; d != NULL; )
	{
	    struct connection *next = d->ac_next;	/* might move underneath us */

	    if (d->kind >= CK_PERMANENT
	    && same_id(&c->spd.that.id, &d->spd.that.id)
	    && !sameaddr(&c->spd.that.host_addr, &d->spd.that.host_addr)
#ifdef DYNAMICDNS
	    && !(c->dnshostname && d->dnshostname && (strcmp(c->dnshostname, d->dnshostname) == 0))
#endif /* DYNAMICDNS */	    
	    )
	    {
		release_connection(d, FALSE);
	    }
	    d = next;
	}
    }
}

/* Find the connection to connection c's peer's client with the
 * largest value of .routing.  All other things being equal,
 * preference is given to c.  If none is routed, return NULL.
 *
 * If erop is non-null, set *erop to a connection sharing both
 * our client subnet and peer's client subnet with the largest value
 * of .routing.  If none is erouted, set *erop to NULL.
 *
 * The return value is used to find other connections sharing a route.
 * *erop is used to find other connections sharing an eroute.
 */
struct connection *
route_owner(struct connection *c
	    , struct spd_route **srp
	    , struct connection **erop
	    , struct spd_route **esrp)
{
    struct connection *d
	, *best_ro = c
	, *best_ero = c;
    struct spd_route *srd, *src;
    struct spd_route *best_sr, *best_esr;
    enum routing_t best_routing, best_erouting;

    passert(oriented(*c));
    best_sr  = NULL;
    best_esr = NULL;
    best_routing = c->spd.routing;
    best_erouting = best_routing;

    for (d = connections; d != NULL; d = d->ac_next)
    {
	for (srd = &d->spd; srd; srd = srd->next)
	{
	    if (srd->routing == RT_UNROUTED)
		continue;

	    for (src = &c->spd; src; src=src->next)
	    {
		if (!samesubnet(&src->that.client, &srd->that.client))
		    continue;
		if (src->that.protocol != srd->that.protocol)
		    continue;
		if (src->that.port != srd->that.port)
		    continue;
		passert(oriented(*d));
		if (srd->routing > best_routing)
		{
		    best_ro = d;
		    best_sr = srd;
		    best_routing = srd->routing;
		}

		if (!samesubnet(&src->this.client, &srd->this.client))
		    continue;
		if (src->this.protocol != srd->this.protocol)
		    continue;
		if (src->this.port != srd->this.port)
		    continue;
		if (srd->routing > best_erouting)
		{
		    best_ero = d;
		    best_esr = srd;
		    best_erouting = srd->routing;
		}
	    }
	}
    }

    DBG(DBG_CONTROL,
	{
	    char cib[CONN_INST_BUF];
	    err_t m = builddiag("route owner of \"%s\"%s %s:"
		, c->name
		, (fmt_conn_instance(c, cib), cib)
		, enum_name(&routing_story, c->spd.routing));

	    if (!routed(best_ro->spd.routing))
		m = builddiag("%s NULL", m);
	    else if (best_ro == c)
		m = builddiag("%s self", m);
	    else
		m = builddiag("%s \"%s\"%s %s", m
		    , best_ro->name
		    , (fmt_conn_instance(best_ro, cib), cib)
		    , enum_name(&routing_story, best_ro->spd.routing));

	    if (erop != NULL)
	    {
		m = builddiag("%s; eroute owner:", m);
		if (!erouted(best_ero->spd.routing))
		    m = builddiag("%s NULL", m);
		else if (best_ero == c)
		    m = builddiag("%s self", m);
		else
		    m = builddiag("%s \"%s\"%s %s", m
			, best_ero->name
			, (fmt_conn_instance(best_ero, cib), cib)
			, enum_name(&routing_story, best_ero->spd.routing));
	    }

	    DBG_log("%s", m);
	});

    if (erop != NULL)
	*erop = erouted(best_erouting)? best_ero : NULL;

    if (srp != NULL )
    {
	*srp = best_sr;
	if (esrp != NULL )
	    *esrp = best_esr;
    }

    return routed(best_routing)? best_ro : NULL;
}

/* Find a connection that owns the shunt eroute between subnets.
 * There ought to be only one.
 * This might get to be a bottleneck -- try hashing if it does.
 */
struct connection *
shunt_owner(const ip_subnet *ours, const ip_subnet *his)
{
    struct connection *c;
    struct spd_route *sr;

    for (c = connections; c != NULL; c = c->ac_next)
    {
	for (sr = &c->spd; sr; sr = sr->next)
	{
	    if (shunt_erouted(sr->routing)
	    && samesubnet(ours, &sr->this.client)
	    && samesubnet(his, &sr->that.client))
		return c;
	}
    }
    return NULL;
}

/* Find some connection with this pair of hosts.
 * We don't know enough to chose amongst those available.
 * ??? no longer usefully different from find_host_pair_connections
 */
struct connection *
find_host_connection2(const char *func
		     , const ip_address *me, u_int16_t my_port
		     , const ip_address *him, u_int16_t his_port)
{
    DBG(DBG_CONTROLMORE,
	DBG_log("find_host_connection called from %s", func));
    return find_host_pair_connections(__FUNCTION__, me, my_port, him, his_port);
}

/*
 * extracts the peer's ca from the chained list of public keys
 */
static chunk_t
get_peer_ca(const struct id *peer_id)
{
    struct pubkey_list *p;

    for (p = pubkeys; p != NULL; p = p->next)
    {
       struct pubkey *key = p->key;

       if (key->alg == PUBKEY_ALG_RSA && same_id(peer_id, &key->id))
       {
           return key->issuer;
       }
    }
    return empty_chunk;
}



/* given an up-until-now satisfactory connection, find the best connection
 * now that we just got the Phase 1 Id Payload from the peer.
 *
 * Comments in the code describe the (tricky!) matching criteria.
 * Although this routine could handle the initiator case,
 * it isn't currently called in this case.
 * If it were, it could "upgrade" an Opportunistic Connection
 * to a Road Warrior Connection if a suitable Peer ID were found.
 *
 * In RFC 2409 "The Internet Key Exchange (IKE)",
 * in 5.1 "IKE Phase 1 Authenticated With Signatures", describing Main
 * Mode:
 *
 *         Initiator                          Responder
 *        -----------                        -----------
 *         HDR, SA                     -->
 *                                     <--    HDR, SA
 *         HDR, KE, Ni                 -->
 *                                     <--    HDR, KE, Nr
 *         HDR*, IDii, [ CERT, ] SIG_I -->
 *                                     <--    HDR*, IDir, [ CERT, ] SIG_R
 *
 * In 5.4 "Phase 1 Authenticated With a Pre-Shared Key":
 *
 *               HDR, SA             -->
 *                                   <--    HDR, SA
 *               HDR, KE, Ni         -->
 *                                   <--    HDR, KE, Nr
 *               HDR*, IDii, HASH_I  -->
 *                                   <--    HDR*, IDir, HASH_R
 *
 * refine_host_connection could be called in two case:
 *
 * - the Responder receives the IDii payload:
 *   + [PSK] after using PSK to decode this message
 *   + before sending its IDir payload
 *   + before using its ID in HASH_R computation
 *   + [DSig] before using its private key to sign SIG_R
 *   + before using the Initiator's ID in HASH_I calculation
 *   + [DSig] before using the Initiator's public key to check SIG_I
 *
 * - the Initiator receives the IDir payload:
 *   + [PSK] after using PSK to encode previous message and decode this message
 *   + after sending its IDii payload
 *   + after using its ID in HASH_I computation
 *   + [DSig] after using its private key to sign SIG_I
 *   + before using the Responder's ID to compute HASH_R
 *   + [DSig] before using Responder's public key to check SIG_R
 *
 * refine_host_connection can choose a different connection, as long as
 * nothing already used is changed.
 *
 * In the Initiator case, the particular connection might have been
 * specified by whatever provoked Pluto to initiate.  For example:
 *	whack --initiate connection-name
 * The advantages of switching connections when we're the Initiator seem
 * less important than the disadvantages, so after FreeS/WAN 1.9, we
 * don't do this.
 */
struct connection *
refine_host_connection(const struct state *st, const struct id *peer_id
, bool initiator, bool aggrmode)
{
    struct connection *c = st->st_connection;
    u_int16_t auth = st->st_oakley.auth;
    struct connection *d;
    struct connection *best_found = NULL;
    lset_t auth_policy;
    lset_t p1mode_policy = aggrmode ? POLICY_AGGRESSIVE : LEMPTY;
    const struct RSA_private_key *my_RSA_pri = NULL;
    bool wcpip;	/* wildcard Peer IP? */
    int wildcards, best_wildcards;
    int our_pathlen, best_our_pathlen, peer_pathlen, best_peer_pathlen;
    chunk_t peer_ca;
    const chunk_t *psk;

    psk = NULL;

    our_pathlen = peer_pathlen = 0;
    best_our_pathlen  = 0;
    best_peer_pathlen = 0;
    wildcards = best_wildcards = 0;

    DBG(DBG_CONTROLMORE
	 , DBG_log("refine_connection: starting with %s"
		   , c->name));

    peer_ca = get_peer_ca(peer_id);

    if (same_id(&c->spd.that.id, peer_id)
	&& trusted_ca(peer_ca, c->spd.that.ca, &peer_pathlen)
	&& peer_pathlen == 0
	&& match_requested_ca(c->requested_ca, c->spd.this.ca, &our_pathlen)
	&& our_pathlen == 0
	) {

	DBG(DBG_CONTROLMORE
	    , DBG_log("refine_connection: happy with starting point: %s"
		      , c->name));

	return c;	/* peer ID matches current connection -- look no further */
    }

#if defined(XAUTH)
    auth = xauth_calcbaseauth(auth);
#endif
    switch (auth)
    {
    case OAKLEY_PRESHARED_KEY:
	auth_policy = POLICY_PSK;
	psk = get_preshared_secret(c);
	/* It should be virtually impossible to fail to find PSK:
	 * we just used it to decode the current message!
	 */
	if (psk == NULL)
	    return NULL;	/* cannot determine PSK! */
	break;
		
    case OAKLEY_RSA_SIG:
	auth_policy = POLICY_RSASIG;
	if (initiator && c->spd.this.sc == NULL)
	{
	    /* at this point, we've committed to our RSA private key:
	     * we used it in our previous message.
	     */
	    my_RSA_pri = get_RSA_private_key(c);
	    if (my_RSA_pri == NULL)
		return NULL;	/* cannot determine my RSA private key! */
	}
	break;

    default:
	bad_case(auth);
    }

    /* The current connection won't do: search for one that will.
     * First search for one with the same pair of hosts.
     * If that fails, search for a suitable Road Warrior or Opportunistic
     * connection (i.e. wildcard peer IP).
     * We need to match:
     * - peer_id (slightly complicated by instantiation)
     * - if PSK auth, the key must not change (we used it to decode message)
     * - policy-as-used must be acceptable to new connection
     * - if initiator, also:
     *   + our ID must not change (we sent it in previous message)
     *   + our RSA key must not change (we used in in previous message)
     */
    d = c->host_pair->connections;
    for (wcpip = FALSE; ; wcpip = TRUE)
    {
	for (; d != NULL; d = d->hp_next)
	{
	    bool match1 = match_id(peer_id, &d->spd.that.id, &wildcards);
	    bool match2 = trusted_ca(peer_ca, d->spd.that.ca, &peer_pathlen);
	    bool match3 = match_requested_ca(c->requested_ca, d->spd.this.ca, &our_pathlen);
	    bool match = match1 && match2 && match3;

	    DBG(DBG_CONTROLMORE
		, DBG_log("refine_connection: checking %s against %s, best=%s with match=%d(id=%d/ca=%d/reqca=%d)"
			  , c->name, d->name, best_found ? best_found->name : "(none)"
			  , match, match1, match2, match3));

	    /* ignore group connections */
	    if (d->policy & POLICY_GROUP)
		continue;

	    /* check if peer_id matches, exactly or after instantiation */
	    if (!match)
		continue;

	    /* if initiator, our ID must match exactly */
	    if (initiator && !same_id(&c->spd.this.id, &d->spd.this.id))
		continue;

	    /* authentication used must fit policy of this connection */
	    if ((d->policy & auth_policy) == LEMPTY)
		continue;	/* our auth isn't OK for this connection */

	    if ((d->policy & POLICY_AGGRESSIVE) ^ p1mode_policy)
		continue;   /* disallow phase1 main/aggressive mode mismatch */

#ifdef XAUTH
	    if (d->spd.this.xauth_server != c->spd.this.xauth_server)
		continue;	/* disallow xauth/no xauth mismatch */

	    if (d->spd.this.xauth_client != c->spd.this.xauth_client)
		continue;	/* disallow xauth/no xauth mismatch */
#endif

	    DBG(DBG_CONTROLMORE
		, DBG_log("refine_connection: checked %s against %s, now for see if best"
			  , c->name, d->name));

	    switch (auth)
	    {
	    case OAKLEY_PRESHARED_KEY:
		/* secret must match the one we already used */
		{
		    const chunk_t *dpsk = get_preshared_secret(d);

		    if (dpsk == NULL)
			continue;	/* no secret */

		    if (psk != dpsk)
			if (psk->len != dpsk->len
			|| memcmp(psk->ptr, dpsk->ptr, psk->len) != 0)
			    continue;	/* different secret */
		}
		break;

	    case OAKLEY_RSA_SIG:
		/* We must at least be able to find our private key.
		 * If we initiated, it must match the one we
		 * used in the SIG_I payload that we sent previously.
		 */
 		if (d->spd.this.sc == NULL) /* no smartcard */
		{
		    const struct RSA_private_key *pri
			= get_RSA_private_key(d);

		    if (pri == NULL
		    || (initiator && (c->spd.this.sc != NULL
			|| !same_RSA_public_key(&my_RSA_pri->pub, &pri->pub))))
			    continue;
		}
		else if (initiator && c->spd.this.sc != d->spd.this.sc)
		    continue;
		break;

	    default:
		bad_case(auth);
	    }

	    /* d has passed all the tests.
	     * We'll go with it if the Peer ID was an exact match.
	     */
	    if (match && wildcards == 0 && peer_pathlen == 0 && our_pathlen == 0)
		return d;

	    /* We'll remember it as best_found in case an exact
	     * match doesn't come along.
	     */
	    if (best_found == NULL || wildcards < best_wildcards
	    || ((wildcards == best_wildcards && peer_pathlen < best_peer_pathlen)
		|| (peer_pathlen == best_peer_pathlen && our_pathlen < best_our_pathlen)))
	    {
		DBG(DBG_CONTROLMORE
		    , DBG_log("refine_connection: picking new best %s (wild=%d, peer_pathlen=%d/our=%d)"
			       , d->name
			       , wildcards, peer_pathlen, our_pathlen));
		best_found = d;
		best_wildcards = wildcards;
		best_peer_pathlen = peer_pathlen;
		best_our_pathlen = our_pathlen;
	    }
	}
	if (wcpip)
	    return best_found;	/* been around twice already */

	/* Starting second time around.
	 * We're willing to settle for a connection that needs Peer IP
	 * instantiated: Road Warrior or Opportunistic.
	 * Look on list of connections for host pair with wildcard Peer IP
	 */
	d = find_host_pair_connections(__FUNCTION__, &c->spd.this.host_addr
				       , c->spd.this.host_port
				       , (ip_address *)NULL
				       , c->spd.that.host_port);
    }
}

#ifdef VIRTUAL_IP
/**
 * With virtual addressing, we must not allow someone to use an already
 * used (by another id) addr/net.
 */
static bool
is_virtual_net_used(const ip_subnet *peer_net, const struct id *peer_id)
{
    struct connection *d;
    for (d = connections; d != NULL; d = d->ac_next)
    {
	switch (d->kind) {
            /* It does NOT make sense to check for CK_TEMPLATE entries here,
               since they do not contain a currently used virtual IP address */ 
	    case CK_PERMANENT:
	    case CK_INSTANCE:
		if ((subnetinsubnet(peer_net,&d->spd.that.client) ||
		     subnetinsubnet(&d->spd.that.client,peer_net)) &&
		     !same_id(&d->spd.that.id, peer_id)) {
		    char buf[IDTOA_BUF];
		    char client[SUBNETTOT_BUF];
		    subnettot(peer_net, 0, client, sizeof(client));
		    idtoa(&d->spd.that.id, buf, sizeof(buf));
		    openswan_log("Virtual IP %s is already used by '%s'",
			client, buf);
		    idtoa(peer_id, buf, sizeof(buf));
			openswan_log("Your ID is '%s'", buf);
		    return TRUE; /* already used by another one */
		}
		break;
	    case CK_GOING_AWAY:
	    default:
		break;
	}
    }
    return FALSE; /* you can safely use it */
}
#endif

/* find_client_connection: given a connection suitable for ISAKMP
 * (i.e. the hosts match), find a one suitable for IPSEC
 * (i.e. with matching clients).
 *
 * If we don't find an exact match (not even our current connection),
 * we try for one that still needs instantiation.  Try Road Warrior
 * abstract connections and the Opportunistic abstract connections.
 * This requires inverse instantiation: abstraction.
 *
 * After failing to find an exact match, we abstract the peer
 * to be NO_IP (the wildcard value).  This enables matches with
 * Road Warrior and Opportunistic abstract connections.
 *
 * After failing that search, we also abstract the Phase 1 peer ID
 * if possible.  If the peer's ID was the peer's IP address, we make
 * it NO_ID; instantiation will make it the peer's IP address again.
 *
 * If searching for a Road Warrior abstract connection fails,
 * and conditions are suitable, we search for the best Opportunistic
 * abstract connection.
 *
 * Note: in the end, both Phase 1 IDs must be preserved, after any
 * instantiation.  They are the IDs that have been authenticated.
 */

#define PATH_WEIGHT	1
#define WILD_WEIGHT	(MAX_CA_PATH_LEN+1)
#define PRIO_WEIGHT	(MAX_WILDCARDS+1)*WILD_WEIGHT

/* fc_try: a helper function for find_client_connection */
static struct connection *
fc_try(const struct connection *c
, struct host_pair *hp
, const struct id *peer_id UNUSED
, const ip_subnet *our_net
, const ip_subnet *peer_net
, const u_int8_t our_protocol
, const u_int16_t our_port
, const u_int8_t peer_protocol
, const u_int16_t peer_port)
{
    struct connection *d;
    struct connection *best = NULL;
    policy_prio_t best_prio = BOTTOM_PRIO;
    int wildcards, pathlen;
    const bool peer_net_is_host = subnetisaddr(peer_net, &c->spd.that.host_addr);

    for (d = hp->connections; d != NULL; d = d->hp_next)
    {
	struct spd_route *sr;

	if (d->policy & POLICY_GROUP)
	    continue;

	if (!(same_id(&c->spd.this.id, &d->spd.this.id)
	      && match_id(&c->spd.that.id, &d->spd.that.id, &wildcards)
	      && trusted_ca(c->spd.that.ca, d->spd.that.ca, &pathlen)))
	    continue;

    	/* compare protocol and ports */
	if (d->spd.this.protocol != our_protocol
	    ||  (d->spd.this.port && d->spd.this.port != our_port)
	    ||  d->spd.that.protocol != peer_protocol
            || (d->spd.that.port != peer_port && !d->spd.that.has_port_wildcard))
	    continue;

	/* non-Opportunistic case:
	 * our_client must match.
	 *
	 * So must peer_client, but the testing is complicated
	 * by the fact that the peer might be a wildcard
	 * and if so, the default value of that.client
	 * won't match the default peer_net.  The appropriate test:
	 *
	 * If d has a peer client, it must match peer_net.
	 * If d has no peer client, peer_net must just have peer itself.
	 */

	for (sr = &d->spd; best != d && sr != NULL; sr = sr->next)
	{
	    policy_prio_t prio;
#ifdef DEBUG
	    if (DBGP(DBG_CONTROLMORE))
	    {
		char s1[SUBNETTOT_BUF],d1[SUBNETTOT_BUF];
		char s3[SUBNETTOT_BUF],d3[SUBNETTOT_BUF];

		subnettot(our_net,  0, s1, sizeof(s1));
		subnettot(peer_net, 0, d1, sizeof(d1));
		subnettot(&sr->this.client,  0, s3, sizeof(s3));
		subnettot(&sr->that.client,  0, d3, sizeof(d3));
		DBG_log("  fc_try trying "
			"%s:%s:%d/%d -> %s:%d/%d vs %s:%s:%d/%d -> %s:%d/%d"
			, c->name, s1, c->spd.this.protocol, c->spd.this.port
				 , d1, c->spd.that.protocol, c->spd.that.port
			, d->name, s3, sr->this.protocol, sr->this.port
				 , d3, sr->that.protocol, sr->that.port);
	    }
#endif /* DEBUG */

	    if (!samesubnet(&sr->this.client, our_net))
		continue;

	    if (sr->that.has_client)
	    {
		if (sr->that.has_client_wildcard) {
		    if (!subnetinsubnet(peer_net, &sr->that.client))
			continue;
		} else {
#ifdef VIRTUAL_IP
		    if ((!samesubnet(&sr->that.client, peer_net)) && (!is_virtual_connection(d)))
#else
		    if (!samesubnet(&sr->that.client, peer_net))
#endif
			continue;
#ifdef VIRTUAL_IP
		    if ((is_virtual_connection(d)) &&
			( (!is_virtual_net_allowed(d, peer_net, &c->spd.that.host_addr)) ||
			(is_virtual_net_used(peer_net, peer_id?peer_id:&c->spd.that.id)) ))
			    continue;
#endif
		}
	    }
	    else
	    {
		if (!peer_net_is_host)
		    continue;
	    }

	    /* We've run the gauntlet -- success:
	     * We've got an exact match of subnets.
	     * The connection is feasible, but we continue looking for the best.
	     * The highest priority wins, implementing eroute-like rule.
	     * - a routed connection is preferrred
	     * - given that, the smallest number of ID wildcards are preferred
	     * - given that, the shortest CA pathlength is preferred
	     */
	    prio = PRIO_WEIGHT * routed(sr->routing)
		 + WILD_WEIGHT * (MAX_WILDCARDS - wildcards)
		 + PATH_WEIGHT * (MAX_CA_PATH_LEN - pathlen)
		 + 1;
	    if (prio > best_prio)
	    {
		best = d;
		best_prio = prio;
	    }
	}
    }

    if (best != NULL && NEVER_NEGOTIATE(best->policy))
	best = NULL;

    DBG(DBG_CONTROLMORE,
	DBG_log("  fc_try concluding with %s [%ld]"
		, (best ? best->name : "none"), best_prio)
    )
    return best;
}

static struct connection *
fc_try_oppo(const struct connection *c
, struct host_pair *hp
, const ip_subnet *our_net
, const ip_subnet *peer_net
, const u_int8_t our_protocol
, const u_int16_t our_port
, const u_int8_t peer_protocol
, const u_int16_t peer_port)
{
    struct connection *d;
    struct connection *best = NULL;
    policy_prio_t best_prio = BOTTOM_PRIO;
    int wildcards, pathlen;

    for (d = hp->connections; d != NULL; d = d->hp_next)
    {
	struct spd_route *sr;
	policy_prio_t prio;

	if (d->policy & POLICY_GROUP)
	    continue;

	if (!(same_id(&c->spd.this.id, &d->spd.this.id)
	&& match_id(&c->spd.that.id, &d->spd.that.id, &wildcards)
	&& trusted_ca(c->spd.that.ca, d->spd.that.ca, &pathlen)))
	    continue;

	/* compare protocol and ports */
	if (d->spd.this.protocol != our_protocol
	    ||  (d->spd.this.port && d->spd.this.port != our_port)
	    ||  d->spd.that.protocol != peer_protocol
            || (d->spd.that.port != peer_port && !d->spd.that.has_port_wildcard))
	    continue;

	/* Opportunistic case:
	 * our_net must be inside d->spd.this.client
	 * and peer_net must be inside d->spd.that.client
	 * Note: this host_pair chain also has shunt
	 * eroute conns (clear, drop), but they won't
	 * be marked as opportunistic.
	 */
	for (sr = &d->spd; sr != NULL; sr = sr->next)
	{
#ifdef DEBUG
	    if (DBGP(DBG_CONTROLMORE))
	    {
		char s1[SUBNETTOT_BUF],d1[SUBNETTOT_BUF];
		char s3[SUBNETTOT_BUF],d3[SUBNETTOT_BUF];

		subnettot(our_net,  0, s1, sizeof(s1));
		subnettot(peer_net, 0, d1, sizeof(d1));
		subnettot(&sr->this.client,  0, s3, sizeof(s3));
		subnettot(&sr->that.client,  0, d3, sizeof(d3));
		DBG_log("  fc_try_oppo trying %s:%s -> %s vs %s:%s -> %s"
			, c->name, s1, d1, d->name, s3, d3);
	    }
#endif /* DEBUG */

	    if (!subnetinsubnet(our_net, &sr->this.client)
	    || !subnetinsubnet(peer_net, &sr->that.client))
		continue;

	    /* The connection is feasible, but we continue looking for the best.
	     * The highest priority wins, implementing eroute-like rule.
	     * - our smallest client subnet is preferred (longest mask)
	     * - given that, his smallest client subnet is preferred
	     * - given that, a routed connection is preferrred
	     * - given that, the smallest number of ID wildcards are preferred
	     * - given that, the shortest CA pathlength is preferred
	     */
	    prio = PRIO_WEIGHT * (d->prio + routed(sr->routing))
	    	 + WILD_WEIGHT * (MAX_WILDCARDS - wildcards)
		 + PATH_WEIGHT * (MAX_CA_PATH_LEN - pathlen);
	    if (prio > best_prio)
	    {
		best = d;
		best_prio = prio;
	    }
	}
    }

    /* if the best wasn't opportunistic, we fail: it must be a shunt */
    if (best != NULL
    && (NEVER_NEGOTIATE(best->policy)
	|| (best->policy & POLICY_OPPO) == LEMPTY))
    {
	best = NULL;
    }

    DBG(DBG_CONTROLMORE,
	DBG_log("  fc_try_oppo concluding with %s [%ld]"
		, (best ? best->name : "none"), best_prio)
    )
    return best;

}

struct connection *
find_client_connection(struct connection *c
		       , const ip_subnet *our_net
		       , const ip_subnet *peer_net
		       , const u_int8_t our_protocol
		       , const u_int16_t our_port
		       , const u_int8_t peer_protocol
		       , const u_int16_t peer_port)
{
    struct connection *d;
    struct spd_route *sr;

#ifdef DEBUG
    if (DBGP(DBG_CONTROLMORE))
    {
	char s1[SUBNETTOT_BUF],d1[SUBNETTOT_BUF];

	subnettot(our_net,  0, s1, sizeof(s1));
	subnettot(peer_net, 0, d1, sizeof(d1));

	DBG_log("find_client_connection starting with %s"
	    , (c ? c->name : "(none)"));
	DBG_log("  looking for %s:%d/%d -> %s:%d/%d"
	    , s1, our_protocol, our_port
	    , d1, peer_protocol, peer_port);
    }
#endif /* DEBUG */

    /* give priority to current connection
     * but even greater priority to a routed concrete connection
     */
    {
	struct connection *unrouted = NULL;
	int srnum = -1;

	for (sr = &c->spd; unrouted == NULL && sr != NULL; sr = sr->next)
	{
	    srnum++;

#ifdef DEBUG
	    if (DBGP(DBG_CONTROLMORE))
	    {
		char s2[SUBNETTOT_BUF],d2[SUBNETTOT_BUF];

		subnettot(&sr->this.client, 0, s2, sizeof(s2));
		subnettot(&sr->that.client, 0, d2, sizeof(d2));
		DBG_log("  concrete checking against sr#%d %s -> %s"
			, srnum, s2, d2);
	    }
#endif /* DEBUG */

	    if (samesubnet(&sr->this.client, our_net)
		&& samesubnet(&sr->that.client, peer_net)
		&& (sr->this.protocol == our_protocol)
		&& (!sr->this.port || (sr->this.port == our_port))
		&& (sr->that.protocol == peer_protocol)
		&& (!sr->that.port || (sr->that.port == peer_port)))
	    {
		passert(oriented(*c));
		if (routed(sr->routing))
		    return c;

		unrouted = c;
	    }
	}

	/* exact match? */
	d = fc_try(c, c->host_pair, NULL, our_net, peer_net
	    , our_protocol, our_port, peer_protocol, peer_port);

	DBG(DBG_CONTROLMORE,
	    DBG_log("  fc_try %s gives %s"
		    , c->name
		    , (d ? d->name : "none"))
	)

	if (d == NULL)
	    d = unrouted;
    }

    if (d == NULL)
    {
	/* look for an abstract connection to match */
	struct spd_route *sr;
	struct host_pair *hp = NULL;

	for (sr = &c->spd; hp==NULL && sr != NULL; sr = sr->next)
	{
	    hp = find_host_pair(&sr->this.host_addr
				, sr->this.host_port
				, NULL
				, sr->that.host_port);
#ifdef DEBUG
	    if (DBGP(DBG_CONTROLMORE))
	    {
		char s2[SUBNETTOT_BUF],d2[SUBNETTOT_BUF];

		subnettot(&sr->this.client, 0, s2, sizeof(s2));
		subnettot(&sr->that.client, 0, d2, sizeof(d2));

		DBG_log("  checking hostpair %s -> %s is %s"
			, s2, d2
			, (hp ? "found" : "not found"));
	    }
#endif /* DEBUG */
	}

	if (hp != NULL)
	{
	    /* RW match with actual peer_id or abstract peer_id? */
	    d = fc_try(c, hp, NULL, our_net, peer_net
		, our_protocol, our_port, peer_protocol, peer_port);

	    if (d == NULL
	    && subnetishost(our_net)
	    && subnetishost(peer_net))
	    {
		/* Opportunistic match?
		 * Always use abstract peer_id.
		 * Note that later instantiation will result in the same peer_id.
		 */
		d = fc_try_oppo(c, hp, our_net, peer_net
		    , our_protocol, our_port, peer_protocol, peer_port);
	    }
	}
    }

    DBG(DBG_CONTROLMORE,
	DBG_log("  concluding with d = %s"
		, (d ? d->name : "none"))
    )
    return d;
}

int
connection_compare(const struct connection *ca
, const struct connection *cb)
{
    int ret;

    /* DBG_log("comparing %s to %s", ca->name, cb->name);  */

    ret = strcasecmp(ca->name, cb->name);
    if (ret != 0)
	return ret;

    ret = ca->kind - cb->kind;	/* note: enum connection_kind behaves like int */
    if (ret != 0)
	return ret;

    /* same name, and same type */
    switch (ca->kind)
    {
    case CK_INSTANCE:
	return ca->instance_serial < cb->instance_serial ? -1
	    : ca->instance_serial > cb->instance_serial ? 1
	    : 0;

    default:
	return ca->prio < cb->prio ? -1
	    : ca->prio > cb->prio ? 1
	    : 0;
    }
}

static int
connection_compare_qsort(const void *a, const void *b)
{
    return connection_compare(*(const struct connection *const *)a
			    , *(const struct connection *const *)b);
}

void
show_connections_status(void)
{
    struct connection *c;
    int count, i;
    struct connection **array;


    /* make an array of connections, and sort it */
    count=0;
    for (c = connections; c != NULL; c = c->ac_next)
    {
	count++;
    }
    array = alloc_bytes(sizeof(struct connection *)*count, "connection array");

    count=0;
    for (c = connections; c != NULL; c = c->ac_next)
    {
	array[count++]=c;
    }

    /* sort it! */
    qsort(array, count, sizeof(struct connection *), connection_compare_qsort);

    for (i=0; i<count; i++)
    {
	const char *ifn;
	char instance[1 + 10 + 1];
	char prio[POLICY_PRIO_BUF];

	c = array[i];

	ifn = oriented(*c)? c->interface->ip_dev->id_rname : "";

	instance[0] = '\0';
	if (c->kind == CK_INSTANCE && c->instance_serial != 0)
	    snprintf(instance, sizeof(instance), "[%lu]", c->instance_serial);

	/* show topology */
	{
	    char topo[CONNECTION_BUF];
	    struct spd_route *sr = &c->spd;
	    int num=0;

	    while (sr != NULL)
	    {
		char srcip[ADDRTOT_BUF], dstip[ADDRTOT_BUF];
		char thissemi[3+sizeof("srcup=")];
		char thatsemi[3+sizeof("dstup=")];
		char *thisup, *thatup;

		(void) format_connection(topo, sizeof(topo), c, sr);
		whack_log(RC_COMMENT, "\"%s\"%s: %s; %s; eroute owner: #%lu"
			  , c->name, instance, topo
			  , enum_name(&routing_story, sr->routing)
			  , sr->eroute_owner);
		if(addrbytesptr(&c->spd.this.host_srcip, NULL) == 0
		   || isanyaddr(&c->spd.this.host_srcip)) {
		    strcpy(srcip, "unset");
		} else {
		    addrtot(&sr->this.host_srcip, 0, srcip, sizeof(srcip));
		}
		if(addrbytesptr(&c->spd.that.host_srcip, NULL) == 0
		   || isanyaddr(&c->spd.that.host_srcip)) {
		    strcpy(dstip, "unset");
		} else {
		    addrtot(&sr->that.host_srcip, 0, dstip, sizeof(dstip));
		}

		thissemi[0]='\0';
		thisup=thissemi;
		if(sr->this.updown) {
		    thissemi[0]=';';
		    thissemi[1]=' ';
		    thissemi[2]='\0';
		    strcat(thissemi, "srcup=");
		    thisup=sr->this.updown;
		}
		
		thatsemi[0]='\0';
		thatup=thatsemi;
		if(sr->that.updown) {
		    thatsemi[0]=';';
		    thatsemi[1]=' ';
		    thatsemi[2]='\0';
		    strcat(thatsemi, "dstup=");
		    thatup=sr->that.updown;
		}
		
		whack_log(RC_COMMENT, "\"%s\"%s:     srcip=%s; dstip=%s%s%s%s%s;"
			  , c->name, instance, srcip, dstip
			  , thissemi, thisup
			  , thatsemi, thatup);
		sr = sr->next;
		num++;
	    }
	}

	/* show CAs */
	if (c->spd.this.ca.ptr != NULL || c->spd.that.ca.ptr != NULL)
	{
	    char this_ca[IDTOA_BUF], that_ca[IDTOA_BUF];

	    dntoa_or_null(this_ca, IDTOA_BUF, c->spd.this.ca, "%any");
	    dntoa_or_null(that_ca, IDTOA_BUF, c->spd.that.ca, "%any");

	    whack_log(RC_COMMENT
		, "\"%s\"%s:   CAs: '%s'...'%s'"
		, c->name
		, instance
		, this_ca
		, that_ca);
	}

	whack_log(RC_COMMENT
	    , "\"%s\"%s:   ike_life: %lus; ipsec_life: %lus;"
	    " rekey_margin: %lus; rekey_fuzz: %lu%%; keyingtries: %lu"
	    , c->name
	    , instance
	    , (unsigned long) c->sa_ike_life_seconds
	    , (unsigned long) c->sa_ipsec_life_seconds
	    , (unsigned long) c->sa_rekey_margin
	    , (unsigned long) c->sa_rekey_fuzz
	    , (unsigned long) c->sa_keying_tries);

	if (c->policy_next)
	{
	    whack_log(RC_COMMENT
		      , "\"%s\"%s:   policy_next: %s"
		      , c->name, instance, c->policy_next->name);
	}

	/* Note: we display key_from_DNS_on_demand as if policy [lr]KOD */
	fmt_policy_prio(c->prio, prio);
	whack_log(RC_COMMENT
	    , "\"%s\"%s:   policy: %s%s%s; prio: %s; interface: %s; encap: %s;"
	    , c->name
	    , instance
	    , prettypolicy(c->policy)
	    , c->spd.this.key_from_DNS_on_demand? "+lKOD" : ""
	    , c->spd.that.key_from_DNS_on_demand? "+rKOD" : ""
	    , prio
	    , ifn
		  ,c->forceencaps ? "udp" : "esp");

	/* slightly complicated stuff to avoid extra crap */
	if(c->dpd_timeout > 0 || DBGP(DBG_DPD)) {
	    whack_log(RC_COMMENT
		      , "\"%s\"%s:   dpd: %s; delay:%lu; timeout:%lu; "
		      , c->name
		      , instance
		      , enum_name(&dpd_action_names, c->dpd_action)
		      , c->dpd_delay, c->dpd_timeout);
	}

	if(c->extra_debugging) {
	    whack_log(RC_COMMENT, "\"%s\"%s:   debug: %s"
		      , c->name
		      , instance
		      , bitnamesof(debug_bit_names
				   , c->extra_debugging));
	}

	whack_log(RC_COMMENT
	    , "\"%s\"%s:   newest ISAKMP SA: #%ld; newest IPsec SA: #%ld; "
	    , c->name
	    , instance
	    , c->newest_isakmp_sa
	    , c->newest_ipsec_sa);
#ifdef IKE_ALG
	ike_alg_show_connection(c, instance);
#endif
#ifdef KERNEL_ALG
	kernel_alg_show_connection(c, instance);
#endif
    }
    pfree(array);
}

/* Delete a connection if it is an instance and it is no longer in use.
 * We must be careful to avoid circularity:
 * we don't touch it if it is CK_GOING_AWAY.
 */
void
connection_discard(struct connection *c)
{
    if (c->kind == CK_INSTANCE)
    {
	if(in_pending_use(c)) {
	    return;
	}

	if (!states_use_connection(c))
	    delete_connection(c, FALSE);
    }
}


/* A template connection's eroute can be eclipsed by
 * either a %hold or an eroute for an instance iff
 * the template is a /32 -> /32.  This requires some special casing.
 */

long eclipse_count = 0;

struct connection *
eclipsed(struct connection *c, struct spd_route **esrp)
{
    struct connection *ue;
    struct spd_route *sr1 = &c->spd;

    ue = NULL;

    while (sr1 != NULL && ue != NULL)
    {
	for (ue = connections; ue != NULL; ue = ue->ac_next)
	{
	    struct spd_route *srue = &ue->spd;

	    while (srue != NULL
	    && srue->routing == RT_ROUTED_ECLIPSED
	    && !(samesubnet(&sr1->this.client, &srue->this.client)
		 && samesubnet(&sr1->that.client, &srue->that.client)))
	    {
		srue = srue->next;
	    }
	    if (srue != NULL && srue->routing==RT_ROUTED_ECLIPSED)
	    {
		*esrp = srue;
		break;
	    }
	}
    }
    return ue;
}

#define PENDING_PHASE2_INTERVAL (60*2) /*time between scans of pending phase2*/

/*
 * call me periodically to check to see if pending phase2s ever got
 * unstuck, and if not, perform DPD action.
 */
void connection_check_phase2(void)
{
    struct connection *c, *cnext;
    
    /* reschedule */
    event_schedule(EVENT_PENDING_PHASE2, PENDING_PHASE2_INTERVAL, NULL);

    for (c = connections; c!=NULL; c = cnext) {
	cnext = c->ac_next;

	if(NEVER_NEGOTIATE(c->policy)) {
	    DBG(DBG_CONTROL,
		DBG_log("pending review: connection \"%s\" has no negotiated policy, skipped", c->name));
	    continue;
	}

	if(!(c->policy & POLICY_UP)) {
	    DBG(DBG_CONTROL,
		DBG_log("pending review: connection \"%s\" was not up, skipped", c->name));
	    continue;
	}

	DBG(DBG_CONTROL,
	    DBG_log("pending review: connection \"%s\" checked", c->name));
	
	if(pending_check_timeout(c)) {
	    struct state *p1st;
	    enum connection_kind kind;
	    openswan_log("pending Quick Mode with %s \"%s\" took too long -- replacing phase 1" 
			 , ip_str(&c->spd.that.host_addr)
			 , c->name);

	    kind = c->kind;

	    p1st = find_phase1_state(c, ISAKMP_SA_ESTABLISHED_STATES|PHASE1_INITIATOR_STATES);
	    
	    /* arrange to rekey the phase 1 */
#ifdef DYNAMICDNS
	    if (c->dnshostname != NULL)
		  restart_connections_by_peer(c);
	    else 
	    {
#endif /* DYNAMICDNS */
	    delete_event(p1st);
	    event_schedule(EVENT_SA_REPLACE, 0, p1st);
#ifdef DYNAMICDNS
	    }
#endif /* DYNAMICDNS */
	}
    }
}

void init_connections(void)
{
    event_schedule(EVENT_PENDING_PHASE2, PENDING_PHASE2_INTERVAL, NULL);
}

/*
 * Local Variables:
 * c-basic-offset:4
 * c-style: pluto
 * End:
 */
