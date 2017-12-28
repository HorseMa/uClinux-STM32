/*
 *  Copyright (C) 2007-2008 Sourcefire, Inc.
 *
 *  Authors: Tomasz Kojm
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301, USA.
 */

#ifndef __MATCHER_H
#define __MATCHER_H

#include <sys/types.h>

#include "clamav.h"
#include "filetypes.h"
#include "others.h"
#include "execs.h"
#include "cltypes.h"
#include "md5.h"

#include "matcher-ac.h"
#include "matcher-bm.h"
#include "hashtab.h"

#define CLI_MATCH_WILDCARD	0xff00
#define CLI_MATCH_CHAR		0x0000
#define CLI_MATCH_IGNORE	0x0100
#define CLI_MATCH_ALTERNATIVE	0x0200
#define CLI_MATCH_NIBBLE_HIGH	0x0300
#define CLI_MATCH_NIBBLE_LOW	0x0400

struct cli_matcher {
    /* Extended Boyer-Moore */
    uint8_t *bm_shift;
    struct cli_bm_patt **bm_suffix;
    struct hashset md5_sizes_hs;
    uint32_t *soff, soff_len; /* for PE section sigs */
    uint32_t bm_patterns;

    /* Extended Aho-Corasick */
    uint32_t ac_partsigs, ac_nodes, ac_patterns;
    struct cli_ac_node *ac_root, **ac_nodetable;
    struct cli_ac_patt **ac_pattable;
    uint8_t ac_mindepth, ac_maxdepth;

    uint16_t maxpatlen;
    uint8_t ac_only;
};

struct cli_meta_node {
    char *filename, *virname;
    struct cli_meta_node *next;
    int csize, size, method;
    unsigned int crc32, fileno, encrypted, maxdepth;
};

struct cli_mtarget {
    cli_file_t target;
    const char *name;
    uint8_t idx;    /* idx of matcher */
    uint8_t ac_only;
};

#define CLI_MTARGETS 8
static const struct cli_mtarget cli_mtargets[CLI_MTARGETS] =  {
    { 0,		    "GENERIC",	    0,	0   },
    { CL_TYPE_MSEXE,	    "PE",	    1,	0   },
    { CL_TYPE_MSOLE2,	    "OLE2",	    2,	1   },
    { CL_TYPE_HTML,	    "HTML",	    3,	1   },
    { CL_TYPE_MAIL,	    "MAIL",	    4,	1   },
    { CL_TYPE_GRAPHICS,	    "GRAPHICS",	    5,	1   },
    { CL_TYPE_ELF,	    "ELF",	    6,	1   },
    { CL_TYPE_TEXT_ASCII,   "ASCII",	    7,	1   }
};

struct cli_target_info {
    off_t fsize;
    struct cli_exe_info exeinfo;
    int8_t status; /* 0 == not initialised, 1 == initialised OK, -1 == error */
};

int cli_scanbuff(const unsigned char *buffer, uint32_t length, cli_ctx *ctx, cli_file_t ftype);

int cli_scandesc(int desc, cli_ctx *ctx, cli_file_t ftype, uint8_t ftonly, struct cli_matched_type **ftoffset, unsigned int acmode);

int cli_validatesig(cli_file_t ftype, const char *offstr, off_t fileoff, struct cli_target_info *info, int desc, const char *virname);

off_t cli_caloff(const char *offstr, struct cli_target_info *info, int fd, cli_file_t ftype, int *ret, unsigned int *maxshift);

#endif
