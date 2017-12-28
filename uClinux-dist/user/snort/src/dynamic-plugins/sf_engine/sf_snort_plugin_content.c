/*
 *  sf_snort_plugin_content.c
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
 * Copyright (C) 2005 Sourcefire Inc.
 *
 * Author: Marc Norton
 *         Steve Sturges
 *         Andy Mullican
 *
 * Date: 5/2005
 *
 *
 * Content operations for dynamic rule engine
 */
#include "ctype.h"

#include "sf_snort_packet.h"
#include "sf_snort_plugin_api.h"
#include "sf_dynamic_engine.h"

#include "bmh.h"

extern DynamicEngineData _ded; /* sf_detection_engine.c */
extern int checkCursorInternal(void *p, int flags, int offset, u_int8_t *cursor);

static char *_buffer_end = NULL;
static char *_alt_buffer_end = NULL;
static char *_uri_buffer_end = NULL;

void ContentSetup()
{
    _buffer_end = NULL;
    _alt_buffer_end = NULL;
    _uri_buffer_end = NULL;
}

/*
 *  Initialize Boyer-Moore-Horspool data for single pattern comparisions
 *
 *  returns: 0  -> success
 *           !0 -> error,failed
 */
int BoyerContentSetup(Rule *rule, ContentInfo *content)
{
    /* XXX: need to precompile the B-M stuff */
    
    if( !content->patternByteForm || !content->patternByteFormLength )
        return 0;
    
    content->boyer_ptr = hbm_prep(content->patternByteForm,
        content->patternByteFormLength, 
        content->flags & CONTENT_NOCASE);
    
    if( !content->boyer_ptr )
    {
        /* error doing compilation. */
        _ded.errMsg("Failed to setup pattern match for dynamic rule [%d:%d]\n",
            rule->info.genID, rule->info.sigID);
        return -1;
    }

    return 0;
}


/* 
 *  Content Option processing function
 * 
 *       p: packet data structure, same as the one found in snort.
 * content: data defined in the detection plugin for this rule content option
 *  cursor: updated to point the 1st byte after the match
 *
 * Returns: 
 *    > 0 : match found
 *    = 0 : no match found
 *    < 0 : error
 *
 * Predefined constants: 
 *    (see sf_snort_plugin_api.h for more values)
 *    CONTENT_MATCH   -  if content specifier is found within buffer 
 *    CONTENT_NOMATCH -  if content specifier is not found within buffer 
 * 
 * Notes:
 *   For multiple URI buffers, we scan each buffer, if any one of them 
 *   contains the content we return a match. This is essentially an OR
 *   operation.
 *
 *   Currently support:
 *    options:
 *      nocase
 *      offset
 *      depth
 *    buffers:
 *      normalized(alt-decode)
 *      raw
 *      uri
 *      post
 *      
 */
ENGINE_LINKAGE int contentMatch(void *p, ContentInfo* content, u_int8_t **cursor)
{
    char * q=0;
    char * buffer_start;
    char * buffer_end = NULL;
    u_int  buffer_len;
    int    length;
    int    i;
    char   relative = 0;
    SFSnortPacket *sp = (SFSnortPacket *) p;

    if (content->flags & CONTENT_RELATIVE)
    {
        if( !cursor || !(*cursor) )
        {
            return CONTENT_NOMATCH;
        } 
        relative = 1;
    }

    if (content->flags & (CONTENT_BUF_URI | CONTENT_BUF_POST))
    {
        for (i=0;i<sp->num_uris; i++)
        {
            if ((content->flags & CONTENT_BUF_URI) && (i != HTTP_BUFFER_URI))
            {
                /* Not looking at the "URI" buffer...
                 * keep going. */
                continue;
            }

            if ((content->flags & CONTENT_BUF_POST) && (i != HTTP_BUFFER_CLIENT_BODY))
            {
                /* Not looking at the "POST" buffer...
                 * keep going. */
                continue;
            }

            if (relative)
            {
                if (checkCursorInternal(p, content->flags, content->offset, *cursor) <= 0)
                {
                    /* Okay, cursor is NOT within this buffer... */
                    continue;
                }
                buffer_start = *cursor + content->offset;
            }
            else
            {
                buffer_start = _ded.uriBuffers[i]->uriBuffer + content->offset;
            }

            if (_uri_buffer_end)
            {
                buffer_end = _uri_buffer_end;
            }
            else
            {
                buffer_end = _ded.uriBuffers[i]->uriBuffer + _ded.uriBuffers[i]->uriLength;
            }

            length = buffer_len = buffer_end - buffer_start;

            if (length <= 0)
            {
                continue;
            }
            
            /* Don't bother looking deeper than depth */
            if ( content->depth != 0 && content->depth < buffer_len )
            {
                buffer_len = content->depth;
            }

            q =(char*) hbm_match((HBM_STRUCT*)content->boyer_ptr,buffer_start,buffer_len);

            if (q)
            {
                if (content->flags & CONTENT_END_BUFFER)
                {
                    _uri_buffer_end = q;
                }
                if (cursor)
                {
                    *cursor = q + content->patternByteFormLength;
                }
                return CONTENT_MATCH;
            }
        }

        return CONTENT_NOMATCH;
    }

    if (relative)
    {
        if (checkCursorInternal(p, content->flags, content->offset, *cursor) <= 0)
        {
            return CONTENT_NOMATCH;
        }

        if ((content->flags & CONTENT_BUF_NORMALIZED) && (sp->flags & FLAG_ALT_DECODE))
        {
            if (_alt_buffer_end)
            {
                buffer_end = _alt_buffer_end;
            }
            else
            {         
                buffer_end = _ded.altBuffer + sp->normalized_payload_size;
            }
        }
        else
        {
            if (_buffer_end)
            {
                buffer_end = _buffer_end;
            }
            else
            {
                buffer_end = sp->payload + sp->payload_size;
            }
        }
        buffer_start = *cursor + content->offset;
    }
    else
    {
        if ((content->flags & CONTENT_BUF_NORMALIZED) && (sp->flags & FLAG_ALT_DECODE))
        {
            buffer_start = _ded.altBuffer + content->offset;
            if (_alt_buffer_end)
            {
                buffer_end = _alt_buffer_end;
            }
            else
            {
                buffer_end = _ded.altBuffer + sp->normalized_payload_size;
            }
        }
        else
        {
            buffer_start = sp->payload + content->offset;
            if (_buffer_end)
            {
                buffer_end = _buffer_end;
            }
            else
            {
                buffer_end = sp->payload + sp->payload_size;
            }
        }
    }
    length = buffer_len = buffer_end - buffer_start;

    if (length <= 0)
    {
        return CONTENT_NOMATCH;
    }

    /* Don't bother looking deeper than depth */
    if ( content->depth != 0 && content->depth < buffer_len )
    {
        buffer_len = content->depth;
    }

    q =(char*) hbm_match((HBM_STRUCT*)content->boyer_ptr,buffer_start,buffer_len);

    if (q)
    {
        if (content->flags & CONTENT_END_BUFFER)
        {
            if ((content->flags & CONTENT_BUF_NORMALIZED) && (sp->flags & FLAG_ALT_DECODE))
            {
                _alt_buffer_end = q;
            }
            else
            {
                _buffer_end = q;
            }
        }
        if (cursor)
        {
            *cursor = q + content->patternByteFormLength;
        }
        return CONTENT_MATCH;
    }

    return CONTENT_NOMATCH;
}
