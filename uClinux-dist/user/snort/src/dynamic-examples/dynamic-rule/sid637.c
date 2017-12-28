/*
 * sid637.c
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
 * C-language example for SID 637
 *
 * alert udp $EXTERNAL_NET any -> $HOME_NET any \
 * (msg:"SCAN Webtrends Scanner UDP Probe"; \
 * content:"|0A|help|0A|quite|0A|"; \
 * reference:arachnids,308; classtype:attempted-recon; \
 * sid:637; rev:3;)
 *
 */

/* content:"|0A|help|0A|quite|0A|";  */
static ContentInfo sid637content = 
{
    "|0A|help|0A|quite|0A|",/* pattern to search for */
    0,                      /* depth */
    0,                      /* offset */
    CONTENT_BUF_NORMALIZED, /* flags */
    NULL,                   /* holder for boyer/moore info */
    NULL,                   /* holder for byte representation of "\nhelp\nquite\n" */
    0,                      /* holder for length of byte representation */
    0                       /* holder of increment length */
};

static RuleOption sid637option1 = 
{
    OPTION_TYPE_CONTENT,
    {
        &sid637content
    }
};

/* references for sid 637 */
static RuleReference sid637ref_arachnids =
{
    "arachnids",    /* Type */
    "308"           /* value */
};

static RuleReference *sid637refs[] =
{
    &sid637ref_arachnids,
    NULL
};

RuleOption *sid637options[] =
{
    &sid637option1,
    NULL
};

Rule sid637 =
{
    /* protocol header, akin to => tcp any any -> any any */
    {
        IPPROTO_UDP,        /* proto */
        EXTERNAL_NET,       /* source IP */
        ANY_PORT,           /* source port(s) */
        1,                  /* direction, bi-directional */
        HOME_NET,           /* destination IP */
        ANY_PORT            /* destination port(s) */
    },
    /* metadata */
    { 
        3,                  /* genid -- use 3 to distinguish a C rule */
        637,                /* sigid */
        3,                  /* revision */
        "attempted-recon",  /* classification */
        0,                  /* priority */
       "SCAN Webtrends Scanner UDP Probe",    /* message */
       sid637refs           /* ptr to references */
    },
    sid637options, /* ptr to rule options */
    NULL,                   /* Use internal eval func */
    0,                      /* Holder, not yet initialized, used internally */
    0,                      /* Holder, option count, used internally */
    0,                      /* Holder, no alert used internally for flowbits */
    NULL                    /* Holder, rule data, used internally */
};
