%{
/*
 * Copyright (c) 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 */
#ifndef lint
static const char rcsid[] _U_ =
    "@(#) $Header: /cvs/sw/new-wave/lib/libpcap/grammar-embed.y,v 1.2 2005-09-16 04:50:47 philipc Exp $ (LBL)";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef WIN32
#include <pcap-stdinc.h>
#else /* WIN32 */
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/autoconf.h>
#endif /* WIN32 */

#include <stdlib.h>

#ifndef WIN32
#if __STDC__
struct mbuf;
struct rtentry;
#endif

#include <netinet/in.h>
#endif /* WIN32 */

#include <stdio.h>

#include "pcap-int.h"

#include "gencode.h"
#include "pf.h"
#include <pcap-namedb.h>

#ifdef HAVE_OS_PROTO_H
#include "os-proto.h"
#endif

#define QSET(q, p, d, a) (q).proto = (p),\
			 (q).dir = (d),\
			 (q).addr = (a)

int n_errors = 0;

static struct qual qerr = { Q_UNDEF, Q_UNDEF, Q_UNDEF
#if !CONFIG_SH_SECUREEDGE5410
, Q_UNDEF 
#endif
};

static void
yyerror(char *msg)
{
	++n_errors;
	bpf_error("%s", msg);
	/* NOTREACHED */
}

#ifndef YYBISON
int yyparse(void);

int
pcap_parse()
{
	return (yyparse());
}
#endif

%}

%union {
	int i;
	bpf_u_int32 h;
	u_char *e;
	char *s;
	struct stmt *stmt;
	struct arth *a;
	struct {
		struct qual q;
		int atmfieldtype;
		int mtp3fieldtype;
		struct block *b;
	} blk;
	struct block *rblk;
}

%type	<blk>	expr id nid pid term rterm qid
%type	<blk>	head
%type	<i>	pqual dqual aqual ndaqual
%type	<a>	arth narth
%type	<i>	byteop pname pnum relop irelop
%type	<blk>	and or paren not null prog
%type	<rblk>	other


%token  DST SRC HOST GATEWAY
%token  NET NETMASK PORT PORTRANGE LESS GREATER PROTO PROTOCHAIN CBYTE
%token  ARP RARP IP SCTP TCP UDP ICMP IGMP IGRP PIM VRRP
%token  ATALK AARP DECNET LAT SCA MOPRC MOPDL
%token  TK_BROADCAST TK_MULTICAST
%token  NUM INBOUND OUTBOUND
%token  LINK
%token	GEQ LEQ NEQ
%token	ID EID HID HID6
%token	LSH RSH
%token  LEN
%token  IPV6 ICMPV6 AH ESP
%token	VLAN
%token  ISO ESIS CLNP ISIS L1 L2 IIH LSP SNP CSNP PSNP 
%token  STP
%token  IPX
%token  NETBEUI
%token	RADIO

%type	<s> ID
%type	<e> EID
%type	<s> HID HID6
%type	<i> NUM

%left OR AND
%nonassoc  '!'
%left '|'
%left '&'
%left LSH RSH
%left '+' '-'
%left '*' '/'
%nonassoc UMINUS
%%
prog:	  null expr
{
	finish_parse($2.b);
}
	| null
	;
null:	  /* null */		{ $$.q = qerr; }
	;
expr:	  term
	| expr and term		{ gen_and($1.b, $3.b); $$ = $3; }
	| expr and id		{ gen_and($1.b, $3.b); $$ = $3; }
	| expr or term		{ gen_or($1.b, $3.b); $$ = $3; }
	| expr or id		{ gen_or($1.b, $3.b); $$ = $3; }
	;
and:	  AND			{ $$ = $<blk>0; }
	;
or:	  OR			{ $$ = $<blk>0; }
	;
id:	  nid
	| pnum			{ $$.b = gen_ncode(NULL, (bpf_u_int32)$1,
						   $$.q = $<blk>0.q); }
	| paren pid ')'		{ $$ = $2; }
	;
nid:	  ID			{ $$.b = gen_scode($1, $$.q = $<blk>0.q); }
	| HID '/' NUM		{ $$.b = gen_mcode($1, NULL, $3,
				    $$.q = $<blk>0.q); }
	| HID NETMASK HID	{ $$.b = gen_mcode($1, $3, 0,
				    $$.q = $<blk>0.q); }
	| HID			{
				  /* Decide how to parse HID based on proto */
				  $$.q = $<blk>0.q;
				  $$.b = gen_ncode($1, 0, $$.q);
				}
	| HID6 '/' NUM		{
#ifdef INET6
				  $$.b = gen_mcode6($1, NULL, $3,
				    $$.q = $<blk>0.q);
#else
				  bpf_error("'ip6addr/prefixlen' not supported "
					"in this configuration");
#endif /*INET6*/
				}
	| HID6			{
#ifdef INET6
				  $$.b = gen_mcode6($1, 0, 128,
				    $$.q = $<blk>0.q);
#else
				  bpf_error("'ip6addr' not supported "
					"in this configuration");
#endif /*INET6*/
				}
	| EID			{ 
				  $$.b = gen_ecode($1, $$.q = $<blk>0.q);
				  /*
				   * $1 was allocated by "pcap_ether_aton()",
				   * so we must free it now that we're done
				   * with it.
				   */
				  free($1);
				}
	| not id		{ gen_not($2.b); $$ = $2; }
	;
not:	  '!'			{ $$ = $<blk>0; }
	;
paren:	  '('			{ $$ = $<blk>0; }
	;
pid:	  nid
	| qid and id		{ gen_and($1.b, $3.b); $$ = $3; }
	| qid or id		{ gen_or($1.b, $3.b); $$ = $3; }
	;
qid:	  pnum			{ $$.b = gen_ncode(NULL, (bpf_u_int32)$1,
						   $$.q = $<blk>0.q); }
	| pid
	;
term:	  rterm
	| not term		{ gen_not($2.b); $$ = $2; }
	;
head:	  pqual dqual aqual	{ QSET($$.q, $1, $2, $3); }
	| pqual dqual		{ QSET($$.q, $1, $2, Q_DEFAULT); }
	| pqual aqual		{ QSET($$.q, $1, Q_DEFAULT, $2); }
	| pqual PROTO		{ QSET($$.q, $1, Q_DEFAULT, Q_PROTO); }
	| pqual PROTOCHAIN	{ QSET($$.q, $1, Q_DEFAULT, Q_PROTOCHAIN); }
	| pqual ndaqual		{ QSET($$.q, $1, Q_DEFAULT, $2); }
	;
rterm:	  head id		{ $$ = $2; }
	| paren expr ')'	{ $$.b = $2.b; $$.q = $1.q; }
	| pname			{ $$.b = gen_proto_abbrev($1); $$.q = qerr; }
	| arth relop arth	{ $$.b = gen_relation($2, $1, $3, 0);
				  $$.q = qerr; }
	| arth irelop arth	{ $$.b = gen_relation($2, $1, $3, 1);
				  $$.q = qerr; }
	| other			{ $$.b = $1; $$.q = qerr; }
	;
/* protocol level qualifiers */
pqual:	  pname
	|			{ $$ = Q_DEFAULT; }
	;
/* 'direction' qualifiers */
dqual:	  SRC			{ $$ = Q_SRC; }
	| DST			{ $$ = Q_DST; }
	| SRC OR DST		{ $$ = Q_OR; }
	| DST OR SRC		{ $$ = Q_OR; }
	| SRC AND DST		{ $$ = Q_AND; }
	| DST AND SRC		{ $$ = Q_AND; }
	;
/* address type qualifiers */
aqual:	  HOST			{ $$ = Q_HOST; }
	| NET			{ $$ = Q_NET; }
	| PORT			{ $$ = Q_PORT; }
	| PORTRANGE		{ $$ = Q_PORTRANGE; }
	;
/* non-directional address type qualifiers */
ndaqual:  GATEWAY		{ $$ = Q_GATEWAY; }
	;
pname:	  LINK			{ $$ = Q_LINK; }
	| IP			{ $$ = Q_IP; }
	| ARP			{ $$ = Q_ARP; }
	| RARP			{ $$ = Q_RARP; }
	| SCTP			{ $$ = Q_SCTP; }
	| TCP			{ $$ = Q_TCP; }
	| UDP			{ $$ = Q_UDP; }
	| ICMP			{ $$ = Q_ICMP; }
	| IGMP			{ $$ = Q_IGMP; }
	| IGRP			{ $$ = Q_IGRP; }
	| PIM			{ $$ = Q_PIM; }
	| VRRP			{ $$ = Q_VRRP; }
	| ATALK			{ $$ = Q_ATALK; }
	| AARP			{ $$ = Q_AARP; }
	| DECNET		{ $$ = Q_DECNET; }
	| LAT			{ $$ = Q_LAT; }
	| SCA			{ $$ = Q_SCA; }
	| MOPDL			{ $$ = Q_MOPDL; }
	| MOPRC			{ $$ = Q_MOPRC; }
	| IPV6			{ $$ = Q_IPV6; }
	| ICMPV6		{ $$ = Q_ICMPV6; }
	| AH			{ $$ = Q_AH; }
	| ESP			{ $$ = Q_ESP; }
	| ISO			{ $$ = Q_ISO; }
	| ESIS			{ $$ = Q_ESIS; }
	| ISIS			{ $$ = Q_ISIS; }
	| L1			{ $$ = Q_ISIS_L1; }
	| L2			{ $$ = Q_ISIS_L2; }
	| IIH			{ $$ = Q_ISIS_IIH; }
	| LSP			{ $$ = Q_ISIS_LSP; }
	| SNP			{ $$ = Q_ISIS_SNP; }
	| PSNP			{ $$ = Q_ISIS_PSNP; }
	| CSNP			{ $$ = Q_ISIS_CSNP; }
	| CLNP			{ $$ = Q_CLNP; }
	| STP			{ $$ = Q_STP; }
	| IPX			{ $$ = Q_IPX; }
	| NETBEUI		{ $$ = Q_NETBEUI; }
	| RADIO			{ $$ = Q_RADIO; }
	;
other:	  pqual TK_BROADCAST	{ $$ = gen_broadcast($1); }
	| pqual TK_MULTICAST	{ $$ = gen_multicast($1); }
	| LESS NUM		{ $$ = gen_less($2); }
	| GREATER NUM		{ $$ = gen_greater($2); }
	| CBYTE NUM byteop NUM	{ $$ = gen_byteop($3, $2, $4); }
	| INBOUND		{ $$ = gen_inbound(0); }
	| OUTBOUND		{ $$ = gen_inbound(1); }
	| VLAN pnum		{ $$ = gen_vlan($2); }
	| VLAN			{ $$ = gen_vlan(-1); }
	;

relop:	  '>'			{ $$ = BPF_JGT; }
	| GEQ			{ $$ = BPF_JGE; }
	| '='			{ $$ = BPF_JEQ; }
	;
irelop:	  LEQ			{ $$ = BPF_JGT; }
	| '<'			{ $$ = BPF_JGE; }
	| NEQ			{ $$ = BPF_JEQ; }
	;
arth:	  pnum			{ $$ = gen_loadi($1); }
	| narth
	;
narth:	  pname '[' arth ']'		{ $$ = gen_load($1, $3, 1); }
	| pname '[' arth ':' NUM ']'	{ $$ = gen_load($1, $3, $5); }
	| arth '+' arth			{ $$ = gen_arth(BPF_ADD, $1, $3); }
	| arth '-' arth			{ $$ = gen_arth(BPF_SUB, $1, $3); }
	| arth '*' arth			{ $$ = gen_arth(BPF_MUL, $1, $3); }
	| arth '/' arth			{ $$ = gen_arth(BPF_DIV, $1, $3); }
	| arth '&' arth			{ $$ = gen_arth(BPF_AND, $1, $3); }
	| arth '|' arth			{ $$ = gen_arth(BPF_OR, $1, $3); }
	| arth LSH arth			{ $$ = gen_arth(BPF_LSH, $1, $3); }
	| arth RSH arth			{ $$ = gen_arth(BPF_RSH, $1, $3); }
	| '-' arth %prec UMINUS		{ $$ = gen_neg($2); }
	| paren narth ')'		{ $$ = $2; }
	| LEN				{ $$ = gen_loadlen(); }
	;
byteop:	  '&'			{ $$ = '&'; }
	| '|'			{ $$ = '|'; }
	| '<'			{ $$ = '<'; }
	| '>'			{ $$ = '>'; }
	| '='			{ $$ = '='; }
	;
pnum:	  NUM
	| paren pnum ')'	{ $$ = $2; }
	;
%%
