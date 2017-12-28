/*
 * sid109.c
 *
 * Copyright (C) 2006 Sourcefire,Inc
 * Steven A. Sturges <ssturges@sourcefire.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Description:
 *
 * This file is part of an example of a dynamically loadable rules library.
 *
 * NOTES:
 *
 */

#include "sf_snort_plugin_api.h"
#include "sf_snort_packet.h"
#include "detection_lib_meta.h"

/*
 * C-language example for SID 109
 *
 * alert tcp $HOME_NET 12345:12346 -> $EXTERNAL_NET any \
 * (msg:"BACKDOOR netbus active"; flow:from_server,established; \
 * content:"NetBus"; reference:arachnids,401; classtype:misc-activity; \
 * sid:109; rev:5;)
 *
 */

/* flow:established, from_server; */
static FlowFlags sid109flow = 
{
    FLOW_ESTABLISHED|FLOW_TO_CLIENT
};

static RuleOption sid109option1 =
{
    OPTION_TYPE_FLOWFLAGS,
    {
        &sid109flow
    }
};

/* content:"NetBus";  */
static ContentInfo sid109content = 
{
    "NetBus",               /* pattern to search for */
    0,                      /* depth */
    0,                      /* offset */
    CONTENT_BUF_NORMALIZED, /* flags */
    NULL,                   /* holder for boyer/moore info */
    NULL,                   /* holder for byte representation of "NetBus" */
    0,                      /* holder for length of byte representation */
    0                       /* holder of increment length */
};

static RuleOption sid109option2 = 
{
    OPTION_TYPE_CONTENT,
    {
        &sid109content
    }
};

/* references for sid 109 */
static RuleReference sid109ref_arachnids =
{
    "arachnids",    /* Type */
    "401"           /* value */
};

static RuleReference *sid109refs[] =
{
    &sid109ref_arachnids,
    NULL
};

RuleOption *sid109options[] =
{
    &sid109option1,
    &sid109option2,
    NULL
};

Rule sid109 =
{
    /* protocol header, akin to => tcp any any -> any any */
    {
        IPPROTO_TCP,        /* proto */
        HOME_NET,           /* source IP */
        "12345:12346",      /* source port(s) */
        0,                  /* direction, uni-directional */
        EXTERNAL_NET,       /* destination IP */
        ANY_PORT            /* destination port(s) */
    },
    /* metadata */
    { 
        3,                  /* genid -- use 3 to distinguish a C rule */
        109,                /* sigid */
        5,                  /* revision */
        "misc-activity",    /* classification */
        0,                  /* priority */
       "BACKDOOR netbus active",    /* message */
       sid109refs           /* ptr to references */
    },
    sid109options, /* ptr to rule options */
    NULL,                   /* Use internal eval func */
    0,                      /* Holder, not yet initialized, used internally */
    0,                      /* Holder, option count, used internally */
    0,                      /* Holder, no alert used internally for flowbits */
    NULL                    /* Holder, rule data, used internally */
};
