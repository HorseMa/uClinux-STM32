/* $Id$ */
/*
 ** Copyright (C) 1998-2006 Sourcefire, Inc.
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** You should have received a copy of the GNU General Public License
 ** along with this program; if not, write to the Free Software
 ** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* sp_isdataat
 * 
 * Purpose:
 *    Test a specific byte to see if there is data.  (Basicly, rule keyword
 *    into inBounds)
 *
 * Arguments:
 *    <int>         byte location to check if there is data
 *    ["relative"]  look for byte location relative to the end of the last
 *                  pattern match
 *    ["rawbytes"]  force use of the non-normalized buffer.
 *   
 * Sample:
 *   alert tcp any any -> any 110 (msg:"POP3 user overflow"; \
 *      content:"USER"; isdataat:30,relative; content:!"|0a|"; within:30;)
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <stdlib.h>
#include <ctype.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#include <errno.h>

#include "bounds.h"
#include "rules.h"
#include "decode.h"
#include "plugbase.h"
#include "parser.h"
#include "debug.h"
#include "util.h"
#include "mstring.h"

extern char *file_name;  /* this is the file name from rules.c, generally used
                            for error messages */

extern int file_line;    /* this is the file line number from rules.c that is
                            used to indicate file lines for error messages */

extern u_int8_t *doe_ptr;

#define ISDATAAT_RELATIVE_FLAG 0x01
#define ISDATAAT_RAWBYTES_FLAG 0x02

typedef struct _IsDataAtData
{
    u_int32_t offset;        /* byte location into the packet */
    u_int8_t  flags;         /* relative to the doe_ptr? */
                             /* rawbytes buffer? */
} IsDataAtData;

extern u_int8_t DecodeBuffer[DECODE_BLEN];

void IsDataAtInit(char *, OptTreeNode *, int);
void IsDataAtParse(char *, IsDataAtData *, OptTreeNode *);
int  IsDataAt(Packet *, struct _OptTreeNode *, OptFpList *);

/****************************************************************************
 * 
 * Function: SetupIsDataAt()
 *
 * Purpose: Load 'er up
 *
 * Arguments: None.
 *
 * Returns: void function
 *
 ****************************************************************************/
void SetupIsDataAt(void)
{
    /* map the keyword to an initialization/processing function */
    RegisterPlugin("isdataat", IsDataAtInit);

    DEBUG_WRAP(DebugMessage(DEBUG_PLUGIN,"Plugin: IsDataAt Setup\n"););
}


/****************************************************************************
 * 
 * Function: IsDataAt(char *, OptTreeNode *, int protocol)
 *
 * Purpose: Generic rule configuration function.  Handles parsing the rule 
 *          information and attaching the associated detection function to
 *          the OTN.
 *
 * Arguments: data => rule arguments/data
 *            otn => pointer to the current rule option list node
 *            protocol => protocol the rule is on (we don't care in this case)
 *
 * Returns: void function
 *
 ****************************************************************************/
void IsDataAtInit(char *data, OptTreeNode *otn, int protocol)
{
    IsDataAtData *idx;
    OptFpList *fpl;

    /* allocate the data structure and attach it to the
       rule's data struct list */
    idx = (IsDataAtData *) SnortAlloc(sizeof(IsDataAtData));

    if(idx == NULL)
    {
        FatalError("%s(%d): Unable to allocate IsDataAt data node\n", 
                file_name, file_line);
    }

    /* this is where the keyword arguments are processed and placed into the 
       rule option's data structure */
    IsDataAtParse(data, idx, otn);

    fpl = AddOptFuncToList(IsDataAt, otn);
    
    /* attach it to the context node so that we can call each instance
     * individually
     */
    fpl->context = (void *) idx;

    if (idx->flags & ISDATAAT_RELATIVE_FLAG)
        fpl->isRelative = 1;
}



/****************************************************************************
 * 
 * Function: IsDataAt(char *, IsDataAtData *, OptTreeNode *)
 *
 * Purpose: This is the function that is used to process the option keyword's
 *          arguments and attach them to the rule's data structures.
 *
 * Arguments: data => argument data
 *            idx => pointer to the processed argument storage
 *            otn => pointer to the current rule's OTN
 *
 * Returns: void function
 *
 ****************************************************************************/
void IsDataAtParse(char *data, IsDataAtData *idx, OptTreeNode *otn)
{
    char **toks;
    int num_toks;
    int i;
    char *cptr;
    char *endp;

    toks = mSplit(data, ",", 3, &num_toks, 0);

    if(num_toks > 3) 
        FatalError("ERROR %s (%d): Bad arguments to IsDataAt: %s\n", file_name,
                file_line, data);

    /* set how many bytes to process from the packet */
    idx->offset = strtol(toks[0], &endp, 10);

    if(toks[0] == endp)
    {
        FatalError("%s(%d): Unable to parse as byte value %s\n",
                   file_name, file_line, toks[0]);
    }

    if(idx->offset > 65535)
    {
        FatalError("%s(%d): IsDataAt offset greater than max IPV4 packet size",
                file_name, file_line);
    }

    for (i=1; i< num_toks; i++)
    {
        cptr = toks[i];

        while(isspace((int)*cptr)) {cptr++;}

        if(!strcasecmp(cptr, "relative"))
        {
            /* the offset is relative to the last pattern match */
            idx->flags |= ISDATAAT_RELATIVE_FLAG;
        }
        else if(!strcasecmp(cptr, "rawbytes"))
        {
            /* the offset is to be applied to the non-normalized buffer */
            idx->flags |= ISDATAAT_RAWBYTES_FLAG;
        }
        else
        {
            FatalError("%s(%d): unknown modifier \"%s\"\n",
                    file_name, file_line, toks[1]);
        }
    }

    mSplitFree(&toks,num_toks);
}


/****************************************************************************
 * 
 * Function: IsDataAt(char *, OptTreeNode *, OptFpList *)
 *
 * Purpose: Use this function to perform the particular detection routine
 *          that this rule keyword is supposed to encompass.
 *
 * Arguments: p => pointer to the decoded packet
 *            otn => pointer to the current rule's OTN
 *            fp_list => pointer to the function pointer list
 *
 * Returns: If the detection test fails, this function *must* return a zero!
 *          On success, it calls the next function in the detection list 
 *
 ****************************************************************************/
int IsDataAt(Packet *p, struct _OptTreeNode *otn, OptFpList *fp_list)
{
    IsDataAtData *isdata;
    int dsize;
    char *base_ptr, *end_ptr, *start_ptr;

    isdata = (IsDataAtData *) fp_list->context;

    if (!isdata)
    {
        return 0;
    }

    if (isdata->flags & ISDATAAT_RAWBYTES_FLAG)
    {
        /* Rawbytes specified, force use of that buffer */
        dsize = p->dsize;
        start_ptr = (char *) p->data;
        DEBUG_WRAP(DebugMessage(DEBUG_PATTERN_MATCH, 
                    "Using RAWBYTES buffer!\n"););
    }
    else if(p->packet_flags & PKT_ALT_DECODE)
    {
        /* If normalized buffer available, use it... */
        dsize = p->alt_dsize;
        start_ptr = (char *)DecodeBuffer;
        DEBUG_WRAP(DebugMessage(DEBUG_PATTERN_MATCH, 
                    "Using Alternative Decode buffer!\n"););
    }
    else
    {
        dsize = p->dsize;
        start_ptr = (char *) p->data;
    }

    base_ptr = start_ptr;
    end_ptr = start_ptr + dsize;
    
    if(doe_ptr)
    {
        if(!inBounds(start_ptr, end_ptr, doe_ptr))
        {
            DEBUG_WRAP(DebugMessage(DEBUG_PATTERN_MATCH,
                                    "[*] isdataat bounds check failed..\n"););
            return 0;
        }
    }

    if((isdata->flags & ISDATAAT_RELATIVE_FLAG) && doe_ptr)
    {
        DEBUG_WRAP(DebugMessage(DEBUG_PATTERN_MATCH,
                                "Checking relative offset!\n"););
        base_ptr = doe_ptr + isdata->offset;
    }
    else
    {
        DEBUG_WRAP(DebugMessage(DEBUG_PATTERN_MATCH,
                                "checking absolute offset %d\n", isdata->offset););
        base_ptr = start_ptr + isdata->offset;
    }

    if(inBounds(start_ptr, end_ptr, base_ptr))
    {
        DEBUG_WRAP(DebugMessage(DEBUG_PATTERN_MATCH,
                    "[*] IsDataAt succeeded!  there is data...\n"););
        return fp_list->next->OptTestFunc(p, otn, fp_list->next);
    }


    /* otherwise dump */
    return 0;

}
