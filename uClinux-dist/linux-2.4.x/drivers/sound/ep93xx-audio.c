/*
 * ep93xx-audio.c
 *
 * Common audio handling for the Cirrus EP93xx processor.
 *
 * Copyright (c) 2003 Cirrus Logic Corp.
 * Copyright (C) 2000, 2001 Nicolas Pitre <nico@cam.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License.
 *
 * Taken from sa1100-audio.c
 *
 * This module handles the generic buffering/DMA/mmap audio interface for
 * codecs connected to the EP93xx chip.  All features depending on specific
 * hardware implementations like supported audio formats or samplerates are
 * relegated to separate specific modules.
 *
 *
 * History:
 *
 * 2000-05-21	Nicolas Pitre	Initial release.
 *
 * 2000-06-10	Erik Bunce	Add initial poll support.
 *
 * 2000-08-22	Nicolas Pitre	Removed all DMA stuff. Now using the
 * 				generic SA1100 DMA interface.
 *
 * 2000-11-30	Nicolas Pitre	- Validation of opened instances;
 * 				- Power handling at open/release time instead
 * 				  of driver load/unload;
 *
 * 2001-06-03	Nicolas Pitre	Made this file a separate module, based on
 * 				the former sa1100-uda1341.c driver.
 *
 * 2001-07-22	Nicolas Pitre	- added mmap() and realtime support
 * 				- corrected many details to better comply
 * 				  with the OSS API
 *
 * 2001-10-19	Nicolas Pitre	- brought DMA registration processing
 * 				  into this module for better ressource
 * 				  management.  This also fixes a bug
 * 				  with the suspend/resume logic.
 *
 * 2003-04-04   Adapted for EP93xx I2S/Ac97 audio.
 *
 * 2004-04-23	Added support for multiple stereo streams.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/pm.h>
#include <linux/errno.h>
#include <linux/sound.h>
#include <linux/soundcard.h>
#include <linux/sysrq.h>
#include <linux/delay.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/hardware.h>
#include <asm/semaphore.h>
#include <asm/arch/dma.h>

#include "ep93xx-audio.h"

#undef DEBUG 
// #define DEBUG 1
#ifdef DEBUG
#define DPRINTK( fmt, arg... )  printk( fmt, ##arg )
#else
#define DPRINTK( fmt, arg... )
#endif

/* Mostly just prints what ioctls got called */
/* #define DEBUG 1 */
#ifdef DEBUG
#define DPRINTK_IOCTL( fmt, arg... )  printk( fmt, ##arg )
#else
#define DPRINTK_IOCTL( fmt, arg... )
#endif

/*
 * Experiencing gaps in your audio?  Try upping the AUDIO_NBFRAGS_DEFAULT!
 */
#ifdef CONFIG_SOUND_EP93XX_AC97
#ifdef CONFIG_MACH_IPD
#define AUDIO_NBFRAGS_DEFAULT	32		/* we don't that much memory */
#else
#define AUDIO_NBFRAGS_DEFAULT	128
#endif
#define AUDIO_FRAGSIZE_DEFAULT	4096	/* max is 65536 */
#define AUDIO_NBFRAGS_MAX		128	
#else
#define AUDIO_NBFRAGS_DEFAULT	32
#define AUDIO_FRAGSIZE_DEFAULT	65536	/* max is 65536 */
#define AUDIO_NBFRAGS_MAX		32	
#endif

/*
 * NEXT_BUF
 *
 * Translates to:
 * 		stream->dma_buffer_index++;
 * 		stream->dma_buffer_index %= stream->nbfrags;
 *		stream->dma_buffer = stream->buffers + stream->dma_buffer_index;
 *
 * So stream->dma_buffer always points to the stream->dma_buffer_index-nth element 
 * of stream->buffers.
 */
#define NEXT_BUF(_s_,_b_) { \
	(_s_)->_b_##_index++; \
	(_s_)->_b_##_index %= (_s_)->nbfrags; \
	(_s_)->_b_ = (_s_)->buffers + (_s_)->_b_##_index; }

#define AUDIO_ACTIVE(state)	((state)->rd_ref || (state)->wr_ref)

/* Function prototypes */
static void audio_dma_start( audio_state_t * state, audio_stream_t * stream );
static void audio_dma_pause( audio_state_t * state, audio_stream_t * stream );
static void audio_prime_dma( audio_state_t * state, audio_stream_t * stream );


static __inline__ unsigned long copy_to_user_U16_LE_CM
(
	audio_state_t *state,
	const char *to, 
	const char *from, 
	unsigned long to_count
)
{
	int total_to_count = to_count;
	unsigned short * user_ptr = (unsigned short *)to; /* 16 bit user buffer */
	short * dma_ptr = (short *)from;
	short right, left;
	
	/*
	 * Compact mode - left is the lower 16 bits.  right is the upper 16 bits.
	 */

	if( state->input_stream->audio_num_channels != 1 ) 
	{
		while (to_count > 0) 
		{
			right = *dma_ptr++;
			left  = *dma_ptr++;
			__put_user( left ^ 0x8000, user_ptr++ );
			__put_user( right^ 0x8000, user_ptr++ );
			to_count -= 4;
		}
	}
	else
	{
		while (to_count > 0) 
		{
			dma_ptr++;	/* skip right channel sample */
			left  = *dma_ptr++;
			__put_user( left ^ 0x8000, user_ptr++ );
			to_count -= 2;
		}
	}
	
	return total_to_count;
}

static __inline__ unsigned long copy_to_user_U16_LE
(
	audio_state_t *state,
	const char *to, 
	const char *from, 
	unsigned long to_count
)
{
	int total_to_count = to_count;
	short * user_ptr = (short *)to;	/* 16 bit user buffer */
	int * dma_ptr = (int *)from;	/* 32 bit dma buffer  */

	int shift = state->input_stream->hw_bit_width - 16;
	
	if( state->input_stream->audio_num_channels != 1 ) 
	{
		while (to_count > 0) 
		{
			__put_user( ((short)( (*dma_ptr++) >> shift )) ^ 0x8000, user_ptr++ );
			__put_user( ((short)( (*dma_ptr++) >> shift )) ^ 0x8000, user_ptr++ );
			to_count -= 4;
		}
	}
	else
	{
		while (to_count > 0) 
		{
			__put_user( ((short)( (*dma_ptr++) >> shift )) ^ 0x8000, user_ptr++ );
			dma_ptr++;	/* skip right channel sample */
			to_count -= 2;
		}
	}
	
	return total_to_count;
}

static __inline__ unsigned long copy_to_user_S16_LE_CM
(
	audio_state_t *state,
	const char *to, 
	const char *from, 
	unsigned long to_count
)
{
	int total_to_count = to_count;
	short * user_ptr = (short *)to;	  /* 16 bit user buffer */
	short * dma_ptr = (short *)from;
	short left, right;

	/*
	 * Compact mode - left is the lower 16 bits.  right is the upper 16 bits.
	 */

	if( state->input_stream->audio_num_channels != 1 ) 
	{
		while (to_count > 0) 
		{
			right = *dma_ptr++;
			left  = *dma_ptr++;
			__put_user( left,  user_ptr++ );
			__put_user( right, user_ptr++ );
			to_count -= 4;
		}
	}
	else
	{
		while (to_count > 0) 
		{
			dma_ptr++; /* skip right sample */
			left  = *dma_ptr++;
			__put_user( left,  user_ptr++ );
			to_count -= 2;
		}
	}
	
	return total_to_count;
}

static __inline__ unsigned long copy_to_user_S16_LE
(
	audio_state_t *state,
	const char *to, 
	const char *from, 
	unsigned long to_count
)
{
	int total_to_count = to_count;
	short * user_ptr = (short *)to;	/* 16 bit user buffer */
	int * dma_ptr = (int *)from;	/* 32 bit dma buffer  */

	int shift = state->input_stream->hw_bit_width - 16;
	
	if( state->input_stream->audio_num_channels != 1 ) 
	{
		while (to_count > 0) 
		{
			__put_user( (short)( (*dma_ptr++) >> shift ), user_ptr++ );
			__put_user( (short)( (*dma_ptr++) >> shift ), user_ptr++ );
			to_count -= 4;
		}
	}
	else
	{
		while (to_count > 0) 
		{
			__put_user( (short)((*dma_ptr++) >> shift), user_ptr++ );
			dma_ptr++;	/* skip right channel sample */
			to_count -= 2;
		}
	}
	
	return total_to_count;
}

static __inline__ unsigned long copy_to_user_S8_CM
(
	audio_state_t *state,
	const char *to, 
	const char *from, 
	unsigned long to_count
)
{
	int total_to_count = to_count;
	char * user_ptr = (char *)to;  	/*  8 bit user buffer */
	unsigned short * dma_ptr = (unsigned short *)from;    /* 16 bit dma buffer  */
	char right, left;

	if( state->input_stream->audio_num_channels != 1 ) 
	{
		while (to_count > 0) 
		{
			right = ((char)(*dma_ptr++ >> 8));
			left  = ((char)(*dma_ptr++ >> 8));
			__put_user( left,  user_ptr++ );
			__put_user( right, user_ptr++ );
			to_count -= 2;
		}
	}
	else
	{
		while (to_count > 0) 
		{
			dma_ptr++;	/* skip right channel sample */
			left  = ((char)(*dma_ptr++ >> 8));
			__put_user( left,  user_ptr++ );
			to_count--;
		}
	}
	
	return total_to_count;
}

static __inline__ unsigned long copy_to_user_S8
(
	audio_state_t *state,
	const char *to, 
	const char *from, 
	unsigned long to_count
)
{
	int total_to_count = to_count;
	char * user_ptr = (char *)to;  /*  8 bit user buffer */
	int * dma_ptr = (int *)from;    /* 32 bit dma buffer  */

	int shift = state->input_stream->hw_bit_width - 8;

	if( state->input_stream->audio_num_channels != 1 ) 
	{
		while (to_count > 0) 
		{
			__put_user( (char)( (*dma_ptr++) >> shift ), user_ptr++ );
			__put_user( (char)( (*dma_ptr++) >> shift ), user_ptr++ );
			to_count -= 2;
		}
	}
	else
	{
		while (to_count > 0) 
		{
			__put_user( (char)((*dma_ptr++) >> shift), user_ptr++ );
			dma_ptr++;	/* skip right channel sample */
			to_count--;
		}
	}
	
	return total_to_count;
}

static __inline__ unsigned long copy_to_user_U8_CM
(
	audio_state_t *state,
	const char *to, 
	const char *from, 
	unsigned long to_count
)
{
	int total_to_count = to_count;
	char * user_ptr = (char *)to;  	/*  8 bit user buffer */
	unsigned short * dma_ptr = (unsigned short *)from;    /* 16 bit dma buffer  */
	char right, left;

	if( state->input_stream->audio_num_channels != 1 ) 
	{
		while (to_count > 0) 
		{
			right = ((char)(*dma_ptr++ >> 8)) ^ 0x80;
			left  = ((char)(*dma_ptr++ >> 8)) ^ 0x80;
			__put_user( left,  user_ptr++ );
			__put_user( right, user_ptr++ );
			to_count -= 2;
		}
	}
	else
	{
		while (to_count > 0) 
		{
			dma_ptr++;	/* skip right channel sample */
			left  = ((char)(*dma_ptr++ >> 8)) ^ 0x80;
			__put_user( left,  user_ptr++ );
			to_count--;
		}
	}
	
	return total_to_count;
}

static __inline__ unsigned long copy_to_user_U8
(
	audio_state_t *state,
	const char *to, 
	const char *from, 
	unsigned long to_count
)
{
	int total_to_count = to_count;
	char * user_ptr = (char *)to;  /*  8 bit user buffer */
	int * dma_ptr = (int *)from;    /* 32 bit dma buffer  */

	int shift = state->input_stream->hw_bit_width - 8;

	if( state->input_stream->audio_num_channels != 1 ) 
	{
		while (to_count > 0) 
		{
			__put_user( ((char)( (*dma_ptr++) >> shift )) ^ 0x80, user_ptr++ );
			__put_user( ((char)( (*dma_ptr++) >> shift )) ^ 0x80, user_ptr++ );
			to_count -= 2;
		}
	}
	else
	{
		while (to_count > 0) 
		{
			__put_user( ((char)( (*dma_ptr++) >> shift )) ^ 0x80, user_ptr++ );
			dma_ptr++;	/* skip right channel sample */
			to_count--;
		}
	}
	
	return total_to_count;
}

/*
 * Returns negative for error
 * Returns # of bytes transferred out of the from buffer
 * for success.
 */
static __inline__ int copy_to_user_with_conversion
(
	audio_state_t *state,
	const char *to, 
	const char *from, 
	int toCount
)
{
	int ret=0;
	
	if( toCount == 0 )
	{
		DPRINTK("copy_to_user_with_conversion - nothing to copy!\n");
	}

	/*
	 * Compact Mode means that each 32 bit word has both the
	 * left and right sample in it.
	 */
	if( state->input_stream->bCompactMode )
	{
		switch( state->input_stream->audio_format )
		{
			case AFMT_U8:
				ret = copy_to_user_U8_CM( state, to, from, toCount );
				break;

			case AFMT_S16_LE:
				ret = copy_to_user_S16_LE_CM( state, to, from, toCount );
				break;
		
			case AFMT_S8:
				ret = copy_to_user_S8_CM( state, to, from, toCount );
				break;
				
			case AFMT_U16_LE:
				ret = copy_to_user_U16_LE_CM( state, to, from, toCount );
				break;
				
			case AFMT_S32_BLOCKED:
			default:
				break;
		}
	}
	else
	{
		switch( state->input_stream->audio_format )
		{
			case AFMT_U8:
				ret = copy_to_user_U8( state, to, from, toCount );
				break;

			case AFMT_S16_LE:
				ret = copy_to_user_S16_LE( state, to, from, toCount );
				break;
		
			case AFMT_S8:
				ret = copy_to_user_S8( state, to, from, toCount );
				break;
				
			case AFMT_U16_LE:
				ret = copy_to_user_U16_LE( state, to, from, toCount );
				break;
				
			case AFMT_S32_BLOCKED:
			default:
				__copy_to_user( (char *)to, from, toCount);
				ret = toCount;
				break;
		}
	}

	return ret;
}

static __inline__ int copy_from_user_U16_LE_CM
(
	audio_state_t *state,
	const char *to, 
	const char *from, 
	int toCount 
)
{
	unsigned short * dma_buffer = (unsigned short *)to;
	unsigned short * user_buffer = (unsigned short *)from;
	unsigned short data;
	
	int toCount0 = toCount;
	int mono = (state->output_stream->audio_num_channels == 1);

	/*
	 * Compact mode - left is the lower 16 bits.  right is the upper 16 bits.
	 */
	 
	 if( !mono )
	 {
		while (toCount > 0) 
		{
			__get_user(data, user_buffer++);
			*dma_buffer++ = data ^ 0x8000;
			__get_user(data, user_buffer++);
			*dma_buffer++ = data ^ 0x8000;
			toCount -= 4;
		}
	 }
	 else
	 {
		while (toCount > 0) 
		{
			__get_user(data, user_buffer++);
			data = data ^ 0x8000;
			*dma_buffer++ = data;
			*dma_buffer++ = data;
			toCount -= 4;
		}
	 }
	 

	/*
	 * Each mono or stereo 16 bit sample going in results
	 * in one stereo 32 bit sample going out.  So
	 * mono   = (2 bytes in)/(4 bytes out)
	 * stereo = (4 bytes in)/(4 bytes out)
	 */
	if( mono )
	{
		return toCount0 / 2;
	}
	
	return toCount0;
}

static __inline__ int copy_from_user_U16_LE
(
	audio_state_t *state,
	const char *to, 
	const char *from, 
	int toCount 
)
{
	int * dma_buffer = (int *)to;
	int val;
	unsigned short * user_buffer = (unsigned short *)from;
	unsigned short data;
	
	int toCount0 = toCount;
	int mono = (state->output_stream->audio_num_channels == 1);
	int shift = state->output_stream->hw_bit_width - 16;

	if (!mono) 
	{
		while (toCount > 0) 
		{
			__get_user(data, user_buffer++);
			*dma_buffer++ = ((unsigned int)data ^ 0x8000) << shift;
			__get_user(data, user_buffer++);
			*dma_buffer++ = ((unsigned int)data ^ 0x8000) << shift;
			toCount -= 8;
		}
	}
	else
	{
		while (toCount > 0) 
		{
			__get_user(data, user_buffer++);
			val = ((unsigned int)data ^ 0x8000) << shift;
			*dma_buffer++ = val;
			*dma_buffer++ = val;
			toCount -= 8;
		}
	}
	
	/*
	 * Each mono or stereo 16 bit sample going in results
	 * in a two 32 bit (left & right) samples going out.  So
	 * mono   = (2 bytes in)/(8 bytes out)
	 * stereo = (4 bytes in)/(8 bytes out)
	 */
	if( mono )
		return toCount0 / 4;
	
	return toCount0 / 2;
}

static __inline__ int copy_from_user_S16_LE_CM
(
	audio_state_t *state,
	const char *to, 
	const char *from, 
	int toCount 
)
{
	short * dma_buffer = (short *)to;
	unsigned short * user_buffer = (unsigned short *)from;
	unsigned short data;
	
	int toCount0 = toCount;
	int mono = (state->output_stream->audio_num_channels == 1);

	/*
	 * Compact mode - left is the lower 16 bits.  right is the upper 16 bits.
	 */

	if( !mono )
	{
		__copy_from_user( (char *)to, from, toCount);
	}
	else
	{
		while (toCount > 0) 
		{
			__get_user(data, user_buffer++);
			*dma_buffer++ = data;
			*dma_buffer++ = data;
			toCount -= 4;
		}
	}
	

	/*
	 * Each mono or stereo 16 bit sample going in results
	 * in one stereo 32 bit sample going out.  So
	 * mono   = (2 bytes in)/(4 bytes out)
	 * stereo = (4 bytes in)/(4 bytes out)
	 */
	if( mono )
		return toCount0 / 2;
	
	return toCount0;
}


static __inline__ int copy_from_user_S16_LE
(
	audio_state_t *state,
	const char *to, 
	const char *from, 
	int toCount 
)
{
	int * dma_buffer = (int *)to;
	int val;
	unsigned short * user_buffer = (unsigned short *)from;
	unsigned short data;
	
	int toCount0 = toCount;
	int mono = (state->output_stream->audio_num_channels == 1);
	int shift = state->output_stream->hw_bit_width - 16;

	if (!mono)
	{
		while (toCount > 0) 
		{
			__get_user(data, user_buffer++);
			*dma_buffer++ = (unsigned int)data << shift;
			__get_user(data, user_buffer++);
			*dma_buffer++ = (unsigned int)data << shift;
			toCount -= 8;
		}
	}
	else
	{
		while (toCount > 0) 
		{
			__get_user(data, user_buffer++);
			val = (unsigned int)data << shift;
			*dma_buffer++ = val;
			*dma_buffer++ = val;
			toCount -= 8;
		}
	} 

	/*
	 * Each mono or stereo 16 bit sample going in results
	 * in a two 32 bit samples (left & right) going out.  So
	 * mono   = (2 bytes in)/(8 bytes out)
	 * stereo = (4 bytes in)/(8 bytes out)
	 */
	if( mono )
		return toCount0 / 4;
	
	return toCount0 / 2;
}

static __inline__ int copy_from_user_S8_CM
(
	audio_state_t *state,
	const char *to, 
	const char *from, 
	int toCount 
)
{
	short * dma_buffer = (short *)to;
	short val;
	unsigned char * user_buffer = (unsigned char *)from;
	unsigned char data;
	
	int toCount0 = toCount;
	int mono = (state->output_stream->audio_num_channels == 1);

	/*
	 * Compact mode - left is the lower 16 bits.  right is the upper 16 bits.
	 */

	if (!mono)
	{
		while (toCount > 0) 
		{
			__get_user(data, user_buffer++);
			*dma_buffer++ = (unsigned short)data << 8;
			__get_user(data, user_buffer++);
			*dma_buffer++ = (unsigned short)data << 8;
			toCount -= 4;
		}
	}
	else
	{
		while (toCount > 0) 
		{
			__get_user(data, user_buffer++);
			val = (unsigned short)data << 8;
			*dma_buffer++ = val;
			*dma_buffer++ = val;
			toCount -= 4;
		}
	}
	

	/*
	 * Each mono or stereo 8 bit sample going in results
	 * in one stereo 32 bit sample going out.  So
	 * mono   = (1 byte  in)/(4 bytes out)
	 * stereo = (2 bytes in)/(4 bytes out)
	 */
	if( mono )
		return toCount0 / 4;
	
	return toCount0 / 2;
}

static __inline__ int copy_from_user_S8
(
	audio_state_t *state,
	const char *to, 
	const char *from, 
	int toCount 
)
{
	int * dma_buffer = (int *)to;
	int val;
	unsigned char * user_buffer = (unsigned char *)from;
	unsigned char data;
	
	int toCount0 = toCount;
	int mono = (state->output_stream->audio_num_channels == 1);
	int shift = state->output_stream->hw_bit_width - 8;

	if (!mono) 
	{
		while (toCount > 0) 
		{
			__get_user(data, user_buffer++);
			*dma_buffer++ = (unsigned int)data << shift;
			__get_user(data, user_buffer++);
			*dma_buffer++ = (unsigned int)data << shift;
			toCount -= 8;
		}
	}
	else
	{
		while (toCount > 0) 
		{
			__get_user(data, user_buffer++);
			val = (unsigned int)data << shift;
			*dma_buffer++ = val;
			*dma_buffer++ = val;
			toCount -= 8;
		}
	}
	

	/*
	 * Each mono or stereo 8 bit sample going in results
	 * in a two 32 bit samples (left & right) going out.  So
	 * mono   = (1 byte  in)/(8 bytes out)
	 * stereo = (2 bytes in)/(8 bytes out)
	 */
	if( mono )
		return toCount0 / 8;
	
	return toCount0 / 4;
}

static __inline__ int copy_from_user_U8_CM
(
	audio_state_t *state,
	const char *to, 
	const char *from, 
	int toCount 
)
{
	unsigned short * dma_buffer = (unsigned short *)to;
	unsigned short val;
	unsigned char * user_buffer = (unsigned char *)from;
	unsigned char data;

	int toCount0 = toCount;
	int mono = (state->output_stream->audio_num_channels == 1);

	if (!mono)
	{
		while (toCount > 0) 
		{
			__get_user(data, user_buffer++);
			*dma_buffer++ = ((unsigned short)data ^ 0x80) << 8;
			__get_user(data, user_buffer++);
			*dma_buffer++ = ((unsigned short)data ^ 0x80) << 8;
			toCount -= 4;
		}
	}
	else
	{
		while (toCount > 0) 
		{
			__get_user(data, user_buffer++);
			val = ((unsigned short)data ^ 0x80) << 8;
			*dma_buffer++ = val;
			*dma_buffer++ = val;
			toCount -= 4;
		}
	}
	 

	/*
	 * Each mono or stereo 8 bit sample going in results
	 * in one stereo 32 bit sample going out.  So
	 * mono   = (1 byte  in)/(4 bytes out)
	 * stereo = (2 bytes in)/(4 bytes out)
	 */
	if( mono )
		return toCount0 / 4;
	
	return toCount0 / 2;
}

static __inline__ int copy_from_user_U8
(
	audio_state_t *state,
	const char *to, 
	const char *from, 
	int toCount 
)
{
	unsigned int * dma_buffer = (unsigned int *)to;
	unsigned int val;
	unsigned char * user_buffer = (unsigned char *)from;
	unsigned char data;
	
	int toCount0 = toCount;
	int mono = (state->output_stream->audio_num_channels == 1);
	int shift = state->output_stream->hw_bit_width - 8;

	if (!mono)
	{
		while (toCount > 0) 
		{
			__get_user(data, user_buffer++);
			*dma_buffer++ = ((unsigned int)data ^ 0x80) << shift;
			__get_user(data, user_buffer++);
			*dma_buffer++ = ((unsigned int)data ^ 0x80) << shift;
			toCount -= 8;
		}
	}
	else
	{
		while (toCount > 0) 
		{
			__get_user(data, user_buffer++);
			val = ((unsigned int)data ^ 0x80) << shift;
			*dma_buffer++ = val;
			*dma_buffer++ = val;
			toCount -= 8;
		}
	}
	

	/*
	 * Each mono or stereo 8 bit sample going in results
	 * in a two 32 bit samples (left & right) going out.  So
	 * mono   = (1 byte  in)/(8 bytes out)
	 * stereo = (2 bytes in)/(8 bytes out)
	 */
	if( mono )
		return toCount0 / 8;
	
	return toCount0 / 4;
}

/*
 * Returns negative for error
 * Returns # of bytes transferred out of the from buffer
 * for success.
 */
static __inline__ int copy_from_user_with_conversion
(
	audio_state_t *state,
	const char *to, 
	const char *from, 
	int toCount 
)
{
	int ret=0;
	
	if( toCount == 0 )
	{
		DPRINTK("copy_from_user_with_conversion - nothing to copy!\n");
	}

	/*
	 * Compact Mode means that each 32 bit word has both the
	 * left and right sample in it.
	 */
	if( state->output_stream->bCompactMode )
	{
		switch( state->output_stream->audio_format )
		{
			case AFMT_U8:
				ret = copy_from_user_U8_CM( state, to, from, toCount );
				break;

			case AFMT_S16_LE:
				ret = copy_from_user_S16_LE_CM( state, to, from, toCount );
				break;
		
			case AFMT_S8:
				ret = copy_from_user_S8_CM( state, to, from, toCount );
				break;
				
			case AFMT_U16_LE:
				ret = copy_from_user_U16_LE_CM( state, to, from, toCount );
				break;
				
			case AFMT_S32_BLOCKED:
			default:
				break;
		}
	}
	else
	{
		switch( state->output_stream->audio_format )
		{
			case AFMT_U8:
				ret = copy_from_user_U8( state, to, from, toCount );
				break;

			case AFMT_S16_LE:
				ret = copy_from_user_S16_LE( state, to, from, toCount );
				break;
		
			case AFMT_S8:
				ret = copy_from_user_S8( state, to, from, toCount );
				break;
				
			case AFMT_U16_LE:
				ret = copy_from_user_U16_LE( state, to, from, toCount );
				break;
				
			case AFMT_S32_BLOCKED:
			default:
				__copy_from_user( (char *)to, from, toCount);
				ret = toCount;
				break;
		}
	}
	
	return ret;
}
/*
 *  calculate_dma2usr_ratio_etc()
 *
 *  For audio playback, we convert samples of arbitrary format to be 32 bit 
 *  for our hardware. We're scaling a user buffer to a dma buffer.  So when
 *  report byte counts, we scale them acording to the ratio of DMA sample
 *  size to user buffer sample size.  When we report # of DMA fragments,
 *  we don't scale that.  So:
 *   - The fragment size the app sees  = (stream->fragsize/stream->dma2usr_ratio)
 *   - The # of fragments the app sees = stream->nbfrags
 *
 *	User sample type can be stereo/mono/8/16/32 bit.  
 *  DMA sample type will be either CM (compact mode) where two 16 bit 
 *  samples together in a 32 bit word become a stereo sample or non-CM 
 *  where each channel gets a 32 bit word.
 *
 *  Any time usr sample type changes, we need to call this function.
 *
 *  Also adjust the size and number of dma fragments if sample size changed.
 *
 *  Input format       Input sample     Output sample size    ratio (out:in)
 *  bits   channels    size (bytes)       CM   non-CM          CM   non-CM
 *   8       mono          1               4      8            4:1   8:1
 *   8      stereo         2			   4      8            2:1   4:1
 *   16      mono          2			   4      8            2:1   4:1
 *   16     stereo         4			   4      8            1:1   2:1
 *
 *   24      mono          3			   4      8             X    8:3 not a real case
 *   24     stereo         6			   4      8             X    8:6 not a real case
 *   32      mono          4			   4      8             X    2:1
 *   32     stereo         8			   4      8             X    1:1
 *
 */
static void calculate_dma2usr_ratio_etc( audio_stream_t * stream )
{
	unsigned int dma_sample_size, user_sample_size;
	
	if( stream->bCompactMode )
		dma_sample_size = 4;	/* each stereo sample is 2 * 16 bits */
	else
		dma_sample_size = 8;	/* each stereo sample is 2 * 32 bits */
	
	if( stream->audio_num_channels != 1 )
	{
		// If stereo 16 bit, user sample is 4 bytes.
		// If stereo  8 bit, user sample is 2 bytes.
		user_sample_size = stream->audio_stream_bitwidth / 4;
	}
	else
	{
		// If mono 16 bit, user sample is 2 bytes.
		// If mono  8 bit, user sample is 1 bytes.
		user_sample_size = stream->audio_stream_bitwidth / 8;
	}

	/*
	 * dma2usr_ratio = (4 or 8) / (4, 2, or 1) = 8, 4, 2, or 1
	 */
	stream->dma2usr_ratio = dma_sample_size / user_sample_size;

	DPRINTK(" samplesize: dma %d  user %d  dma/usr ratio  %d\n",
		dma_sample_size, user_sample_size, stream->dma2usr_ratio );
	DPRINTK(" requested: fragsize %d  num frags %d  total bytes %d  total samples %d\n", 
		stream->requested_fragsize, stream->requested_nbfrags,
		stream->requested_fragsize * stream->requested_nbfrags,
		(stream->requested_fragsize * stream->requested_nbfrags) / user_sample_size );
	DPRINTK(" usr      : fragsize %d  num frags %d  total bytes %d  total samples %d\n", 
		(stream->fragsize / stream->dma2usr_ratio),  stream->nbfrags,
		stream->fragsize * stream->nbfrags / stream->dma2usr_ratio,
		(stream->fragsize * stream->nbfrags) / (stream->dma2usr_ratio * user_sample_size) );
	DPRINTK(" dma:       fragsize %d  num frags %d  total bytes %d  total samples %d\n", 
		stream->fragsize,  stream->nbfrags,
		stream->fragsize * stream->nbfrags,
		(stream->fragsize * stream->nbfrags) / dma_sample_size );
		
}

/*
 * audio_deallocate_buffers
 * 
 * This function frees all buffers
 */
static void audio_deallocate_buffers( audio_state_t * state, audio_stream_t * stream )
{
	int frag, i;
	
	DPRINTK("EP93xx - audio_deallocate_buffers\n");

	/* ensure DMA won't run anymore */
	audio_dma_pause( state, stream );
	stream->active = 0;
	stream->stopped = 0;

	for( i=0 ; i < stream->NumDmaChannels ; i++ )
		ep93xx_dma_flush( stream->dmahandles[i] );
	
	if (stream->buffers) 
	{
		for (frag = 0; frag < stream->nbfrags; frag++) 
		{
			if (!stream->buffers[frag].master)
			{
				continue;
			}

			consistent_free(stream->buffers[frag].start,
					stream->buffers[frag].master,
					stream->buffers[frag].dma_addr);
		}

		/*
		 * Free the space allocated to the array of dma_buffer structs.
		 */
		kfree(stream->buffers);
		stream->buffers = NULL;
	}

	stream->buffered_bytes_to_play = 0;
	stream->dma_buffer_index = 0;
	stream->dma_buffer = NULL;
	stream->bytecount = 0;
	stream->getptrCount = 0;
	stream->fragcount = 0;

	DPRINTK("EP93xx - audio_deallocate_buffers - EXIT\n");
}


/*
 * audio_allocate_buffers
 *
 * This function allocates the buffer structure array and buffer data space
 * according to the current number of fragments and fragment size.
 * Note that the output_stream and input_stream structs are allocated
 * in ep93xx-ac97.c or ep93xx-i2s.c.
 */
static int audio_allocate_buffers( audio_state_t * state, audio_stream_t * stream )
{
	int frag;
	int dmasize = 0;
	char *dmabuf = NULL;
	dma_addr_t dmaphys = 0;
	int buf_num = 0;

	if (stream->buffers)
		return -EBUSY;

	DPRINTK("EP93xx audio_allocate_buffers\n" );
	
	/*
	 * Allocate space for the array of audio_buf_t structs.
	 */
	stream->buffers = (audio_buf_t *)
		kmalloc(sizeof(audio_buf_t) * stream->nbfrags, GFP_KERNEL);

	if (!stream->buffers)
	{
		DPRINTK( "ep93xx-audio: unable to allocate audio memory\n ");
		audio_deallocate_buffers( state, stream );
		return -ENOMEM;
	}

	/*
	 * If the audio app hasn't requested a specific fragsize and nbfrags,
	 * we stay with our default.
	 */
	if ((stream->requested_fragsize!=0) && (stream->requested_nbfrags!=0)) {
		/*
		 * Adjust the fragsize to take into account how big actual samples in
		 * the dma buffer are.	Max dma buf size is 64K.
		 */
		stream->fragsize = stream->requested_fragsize * stream->dma2usr_ratio;
	}
	
	if (stream->fragsize > 65536)
		stream->fragsize = 65536;
	if (stream->fragsize < 8192)
		stream->fragsize = 8192;

	/*
	 * Adjust num of frags so we have as many samples of buffer as were requested
	 * even if the fragment size changed.
	 */
	if ((stream->requested_fragsize!=0) && (stream->requested_nbfrags!=0)) {
	  	stream->nbfrags = 
	  		(stream->requested_fragsize * stream->requested_nbfrags * stream->dma2usr_ratio)
	  		 / stream->fragsize;
	}
	
	if (stream->nbfrags < 2)
		stream->nbfrags = 2;
	if (stream->nbfrags > AUDIO_NBFRAGS_MAX)
		stream->nbfrags = AUDIO_NBFRAGS_MAX;

	memset( stream->buffers, 0, sizeof(audio_buf_t) * stream->nbfrags );

	/*
	 * Let's allocate non-cached memory for DMA buffers.
	 * We try to allocate all memory at once.
	 * If this fails (a common reason is memory fragmentation),
	 * then we allocate more smaller buffers.
	 */
	for (frag = 0; frag < stream->nbfrags; frag++) 
	{
		audio_buf_t * dma_buffer = &stream->buffers[frag];

		if (!dmasize) 
		{
			/*
			 * First try to allocate enough for all the frags that
			 * don't yet have memory allocated.
			 */
			dmasize = (stream->nbfrags - frag) * stream->fragsize;
			do {
			  	dmabuf = consistent_alloc(GFP_KERNEL|GFP_DMA,
							  dmasize, &dmaphys);
				
				/* 
				 * If that fails, try to allocate a chunk of memory
				 * that is one less fragment is size.
				 */
				if (!dmabuf)
					dmasize -= stream->fragsize;

			  /* 
			   * Keep trying but the minimum we'll attempt is one 
			   * fragment.  If we can't even get that, give up.
			   */
			} while (!dmabuf && dmasize);
		
			/*
			 * If we do fail to allocate enough for all the frags, 
			 * deallocate whatever we did get and quit.
			 */
			if (!dmabuf)
			{
				DPRINTK( "ep93xx-audio: unable to allocate audio memory\n ");
				audio_deallocate_buffers( state, stream );
				return -ENOMEM;
			}

			DPRINTK("EP93xx allocated %d bytes:  dmabuf=0x%08x  dmaphys=0x%08x\n",
					dmasize, (int)dmabuf, (int)dmaphys );
			
			/*
			 * Success!	 Save the size of this chunk to use when we deallocate it.
			 */
			dma_buffer->master = dmasize;
			memzero(dmabuf, dmasize);
		}

		/*
		 * Save the start address of the dma_buffer fragment.
		 * We know the size of the fragment is fragsize.
		 */
		dma_buffer->start = dmabuf;
		dma_buffer->dma_addr = dmaphys;
		dma_buffer->stream = stream;
		dma_buffer->size = 0;
		dma_buffer->sent = 0;
		dma_buffer->num  = buf_num++;

		sema_init( &dma_buffer->sem, 1 );

		/*
		 * Now if we only allocated the minimal one frag of space, the
		 * dmasize will be ==0 after this subtraction so it will allocate more
		 * for the next frag.  Otherwise, the next time(s) thru this for loop
		 * will dole out frag sized pieces of this big master chunk.
		 */
		dmabuf  += stream->fragsize;
		dmaphys += stream->fragsize;
		dmasize -= stream->fragsize;
	}

	/*
	 * Initialize the stream.
	 */
	stream->buffered_bytes_to_play = 0;
	stream->dma_buffer_index = 0;	/* Init the current buffer index. 			*/
	stream->dma_buffer = &stream->buffers[0];	/* Point dma_buffer to the current buffer struct. 	*/
	stream->bytecount = 0;
	stream->getptrCount = 0;
	stream->fragcount = 0;

	DPRINTK("EP93xx audio_allocate_buffers -- exit SUCCESS\n" );
	return 0;

}


/*
 * audio_reset_buffers
 *
 * This function stops and flushes the dma, gets all buffers back
 * from the DMA driver and resets them ready to be used again.
 */
static void audio_reset_buffers( audio_state_t * state, audio_stream_t * stream )
{
	int frag, i;

	audio_dma_pause( state, stream );
	stream->active = 0;
	stream->stopped = 0;

	for( i=0 ; i < stream->NumDmaChannels ; i++ )
		ep93xx_dma_flush( stream->dmahandles[i] );

	if (stream->buffers) 
	{
		for (frag = 0; frag < stream->nbfrags; frag++) 
		{
			audio_buf_t * dma_buffer = &stream->buffers[frag];
			dma_buffer->size = 0;
			dma_buffer->sent = 0;
			sema_init( &dma_buffer->sem, 1 );
		}
	}

	stream->buffered_bytes_to_play = 0;
	stream->bytecount = 0;
	stream->getptrCount = 0;
	stream->fragcount = 0;
}


/*
 * DMA callback functions
 */
static void audio_dma_tx_callback
( 
	ep93xx_dma_int_t DMAInt,
	ep93xx_dma_dev_t device, 
	unsigned int user_data 
)
{
	unsigned int buf_id;
	int handle, i;
	
	audio_state_t * state = (audio_state_t *)user_data;
	audio_stream_t * stream = state->output_stream;
	
 	/* DPRINTK( "audio_dma_tx_callback - %s\n", stream->devicename ); */
	
	/*
	 * Get the DMA handle that corresponds to the dma channel
	 * that needs servicing.  A multichannel audio stream will
	 * have a DMA handle for each stereo pair that it uses.
	 */
	for( i=0 ; ( i < stream->NumDmaChannels ) && ( stream->dmachannel[i] != device ) ; i++ );
	handle = stream->dmahandles[i];

  	if (stream->mapped)
	{
		/*
		 * If we are mapped, get one dma buffer back and recycle it.
		 */
		if( ep93xx_dma_remove_buffer( handle, &buf_id ) >= 0 )
		{
		   	audio_buf_t * dma_buffer = (audio_buf_t *)buf_id;

	/*	DPRINTK( "audio_dma_tx_callback  - got dma buffer index=%d\n", buf_id); */
		
			/* Accounting */
			stream->buffered_bytes_to_play -= dma_buffer->size;
			
			/* bytecount and fragcount will overflow */
			stream->bytecount += dma_buffer->size;
			if (stream->bytecount < 0)
				stream->bytecount = 0;
			
			stream->fragcount++;
			if (stream->fragcount < 0)
				stream->fragcount = 0;
				
			dma_buffer->size = 0;
		
			/* Recycle dma buffer */
		 	dma_buffer->size = stream->fragsize;
			
			ep93xx_dma_add_buffer( handle, 				/* dma instance 		*/
								   (unsigned int)dma_buffer->dma_addr, /* source 		*/
								   0, 					/* dest 				*/
								   stream->fragsize,	/* size					*/
								   0,					/* is the last chunk? 	*/
								   (unsigned int) dma_buffer ); /* buf id			 	*/	
	  		
			dma_buffer->sent = 1;
		}
	}
	else
	{
		/*
		 * Get all buffers that are free'ed back and clear their semephores.
		 */
		while( ep93xx_dma_remove_buffer( handle, &buf_id ) >= 0 )
		{
		   	audio_buf_t *dma_buffer = (audio_buf_t *)buf_id;

	/*	DPRINTK( "audio_dma_tx_callback  - got dma buffer index=%d\n", buf_id); */
		
			/* Accounting */
			stream->buffered_bytes_to_play -= dma_buffer->size;

			stream->bytecount += dma_buffer->size;
			if (stream->bytecount < 0)
				stream->bytecount = 0;
			
			stream->fragcount++;
			if (stream->fragcount < 0)
				stream->fragcount = 0;

			dma_buffer->size = 0;
			dma_buffer->sent = 0;
			
			/*
			 * Release the semaphore on this dma_buffer.
			 * If write is waiting on this dma_buffer then it can go 
			 * ahead and fill it and send it to the dma.
			 */
			up(&dma_buffer->sem);
		}
	}

	/* And any process polling on write. */
	wake_up(&stream->wq);
}

static void audio_dma_rx_callback
( 
	ep93xx_dma_int_t DMAInt,
	ep93xx_dma_dev_t device, 
	unsigned int user_data 
)
{
	unsigned int buf_id;
	int handle, i;
	
	audio_state_t * state = (audio_state_t *)user_data;
	audio_stream_t * stream = state->input_stream;

	//DPRINTK("audio_dma_rx_callback\n");
	
	/*
	 * Get the DMA handle that corresponds to the dma channel
	 * that needs servicing.  A multichannel audio stream will
	 * have a DMA handle for each stereo pair that it uses.
	 */
	for( i=0 ; ( i < stream->NumDmaChannels ) && ( stream->dmachannel[i] != device ) ; i++ );
	handle = stream->dmahandles[i];
	
	/*
	 * Keep removing and recycling buffers as long as there are buffers
	 * to remove.
	 */
	while( !ep93xx_dma_remove_buffer( handle, &buf_id ) )
	{
	   	audio_buf_t *dma_buffer = (audio_buf_t *) buf_id;

		/* Accounting */
		stream->bytecount += stream->fragsize;
		if (stream->bytecount < 0)
			stream->bytecount = 0;

		stream->fragcount++;
		if (stream->fragcount < 0)
			stream->fragcount = 0;

		/* Recycle dma buffer */
		if( stream->mapped ) 
		{
		 	ep93xx_dma_add_buffer( handle,			 	/* dma instance 		*/
								   (unsigned int) dma_buffer->dma_addr, /* source 		*/
								   0, 					/* dest 				*/
								   stream->fragsize,	/* size					*/
								   0,					/* is the last chunk? 	*/
								   (unsigned int) dma_buffer ); /* buf id			 	*/	
		} 
		else 
		{
			dma_buffer->size = stream->fragsize;
			up(&dma_buffer->sem);
		}

		/* And any process polling on write. */
		wake_up(&stream->wq);
	}
}

/*
 * audio_sync
 *
 * Wait until the last byte written to this device has been played.
 */
static int audio_sync(struct file *file)
{
	audio_state_t *state = (audio_state_t *)file->private_data;
	audio_stream_t * stream = state->output_stream;
	audio_buf_t * dma_buffer;
	int buf_wait_index;

	DPRINTK( "audio_sync - enter\n" );

	if (!(file->f_mode & FMODE_WRITE) || !stream->buffers || stream->mapped)
	{
		DPRINTK( "audio_sync - exit immediately.\n" );
		return 0;
	}
	
	/*
	 * Send current dma buffer if it contains data and hasn't been sent. 
	 */
	dma_buffer = stream->dma_buffer;
	
	if( dma_buffer->size && !dma_buffer->sent ) 
	{
		DPRINTK("audio_sync -- SENDING BUFFER index=%d size=%d\n",
				stream->dma_buffer_index, dma_buffer->size );
		
		down(&dma_buffer->sem);
	 	ep93xx_dma_add_buffer( stream->dmahandles[0],				/* dma instance 		*/
							   (unsigned int)dma_buffer->dma_addr, 	/* source 		*/
							   0, 									/* dest 				*/
							   dma_buffer->size,					/* size					*/
							   0,									/* is the last chunk? 	*/
							   (unsigned int) dma_buffer ); 		/* dma_buffer id			 	*/	
	 	dma_buffer->sent = 1;
		NEXT_BUF(stream, dma_buffer);
	}

	/*
	 * Let's wait for the last dma buffer we sent i.e. the one before the
	 * current dma_buffer_index.  When we acquire the semaphore, this means either:
	 * - DMA on the buffer completed or
	 * - the buffer was already free thus nothing else to sync.
	 */
	buf_wait_index = ((stream->nbfrags + stream->dma_buffer_index - 1) % stream->nbfrags);
	dma_buffer = stream->buffers + buf_wait_index;

	DPRINTK("audio_sync - waiting on down_interruptible\n");
	if (down_interruptible(&dma_buffer->sem))
	{
		DPRINTK("audio_sync - EXIT - down_interruptible\n");
		return -EINTR;
	}
	
	up(&dma_buffer->sem);
	
	DPRINTK("audio_sync - EXIT\n");
	return 0;
}

//static int iWriteCount = 0;

/*
 * Need to convert to 32 bit stereo format:	  
 * 16 bit signed
 * 16 bit unsigned
 *  8 bit signed
 *  8 bit unsigned
 */
static int audio_write
(
	struct file 	*file, 
	const char 		*user_buffer,
	size_t 			src_count, 
	loff_t 			*ppos
)
{
	const char * user_buffer_start = user_buffer;
	audio_state_t * state = (audio_state_t *)file->private_data;
	audio_stream_t * stream = state->output_stream;
	unsigned int dma_xfer_count, src_xfer_count, expanded_count; 
	int ret = 0;
//	int i,j;
	
 	/* DPRINTK_IOCTL( "EP93xx - audio_write: count=%d\n", src_count ); */

	if (ppos != &file->f_pos)
		return -ESPIPE;
		
	if (stream->mapped)
		return -ENXIO;
		
	if( !access_ok( VERIFY_READ, user_buffer, src_count ) )
	{
		DPRINTK( "access_ok failed for audio_write!!!!\n" );
		return -EFAULT;
	}

	/*
	 * Allocate dma buffers if we haven't already done so.
	 */
	if( !stream->buffers && audio_allocate_buffers( state, stream ) )
		return -ENOMEM;

//	if( iWriteCount > 40 )
//	{
//		for( i=0; i<4 ; i++ )
//		{
//			for( j=0; j<8 ; j++ )
//			{
//				printk("%02x ", user_buffer[(i*8)+j] );
//			}
//			printk("\n");
//		}
//		iWriteCount = 0;
//	}
//	iWriteCount++;
	
	/*
	 * Stay in this loop until we have copied all of the file
	 * into user memory.
	 */
	while( src_count > 0 ) 
	{
		audio_buf_t *dma_buffer = stream->dma_buffer;

		/* Wait for a dma buffer to become free */
		if (file->f_flags & O_NONBLOCK)
		{
			ret = -EAGAIN;
			if (down_trylock(&dma_buffer->sem))
				break;
		} 
		else 
		{
			ret = -ERESTARTSYS;
			if (down_interruptible(&dma_buffer->sem))
				break;
		}

		/* 
		 * Feed the current dma buffer (stream->dma_buffer) 
		 * This involves expanding sample size from the user_buffer
		 * to be 32 bit stereo for our dma.
		 */
		
		/*
		 * How much space is left in the current dma buffer?
		 *
		 * dma_xfer_count is # of bytes placed into the dma buffer
		 * where each sample is 8 bytes (4 bytes left, 4 bytes right)
		 * because that's what the I2S is set up for - 32 bit samples.
		 */
		dma_xfer_count = stream->fragsize - dma_buffer->size;
		
		/*
		 * user_buffer is src_count (bytes) of whatever format.
		 * How big will src_count be when sample size is expanded
		 * to 32 bit samples for our hardware?
		 */
		expanded_count = src_count * stream->dma2usr_ratio;
		
		/*
		 * See if we can fit all the remaining user_buffer in
		 * the current dma buffer...
		 */
		if (dma_xfer_count > expanded_count)
			dma_xfer_count = expanded_count;
		
		//DPRINTK( "EP93xx - audio_write: %d to dma_buffer # %d\n", dma_xfer_count, stream->dma_buffer_index );
		
		src_xfer_count = copy_from_user_with_conversion( state, 
											dma_buffer->start + dma_buffer->size, 
											user_buffer,
											dma_xfer_count);
		if( src_xfer_count <= 0 ) 
		{
			up(&dma_buffer->sem);
			return -EFAULT;
		}

		/*
		 * Increment dma buffer pointer.
		 */
		dma_buffer->size += dma_xfer_count;

		/* 
		 * Increment user_buffer pointer.
		 * Decrement the user_buffer size count.
		 */
		user_buffer += src_xfer_count;
		src_count  -= src_xfer_count;

		/*
		 * If the chunk is less than a whole fragment, don't
		 * send it, wait for more ??? I say, go ahead and send it.
		 */
#if 0
		if (dma_buffer->size < stream->fragsize) 
		{
			up(&dma_buffer->sem);
			break;
		}	 
#endif

		/*
		 * If we haven't already started the DMA start it.
		 * But don't start it if we are waiting on a trigger.
		 */
		if( !stream->active && !stream->stopped )
		{
			stream->active = 1;
			audio_dma_start( state, stream );
		}
		
		/*
		 * Note that we've 'downed' the semiphore for this buffer.  But the
		 * dma driver doesn't know about it until we've added it to the DMA
		 * driver's buffer queue.  So let's do that now.
		 */
	 	ep93xx_dma_add_buffer( stream->dmahandles[0], 			/* dma instance 		*/
							   (unsigned int)dma_buffer->dma_addr, /* source 		*/
							   0, 							/* dest 				*/
							   dma_buffer->size,			/* size					*/
							   0,							/* is the last chunk? 	*/
							   (unsigned int) dma_buffer ); /* buf id			 	*/	

		/*
		 * Note that we added a buffer to play for the benefit of calculating
		 * ODELAY.
		 */
		stream->buffered_bytes_to_play += dma_buffer->size;
		
		/* 
		 * Indicate that the dma buffer has been sent.  Not the same as the
		 * buffer's semiphore.
		 */
		dma_buffer->sent = 1;
		
		NEXT_BUF(stream, dma_buffer);
	}
	
	/*
	 * Return the number of bytes transferred.
	 */
	if( (int)user_buffer - (int)user_buffer_start )
		ret = (int)user_buffer - (int)user_buffer_start;
	
/*	DPRINTK( "EP93xx - audio_write: return=%d\n", ret );*/
	return ret;
}

/*
 * audio_dma_start
 *
 * Our Ac97 has a specific start order that it likes.  Enable the 
 * Ac97 channel AFTER enabling DMA.  Our I2S is not so picky.
 */
void audio_dma_start( audio_state_t * state, audio_stream_t * stream )
{
	ep93xx_dma_start( stream->dmahandles[0],
					  stream->NumDmaChannels, 
					  stream->dmahandles );
	
	if (state->hw->hw_enable)
		state->hw->hw_enable( stream );

}

/*
 * audio_dma_pause
 */
void audio_dma_pause( audio_state_t * state, audio_stream_t * stream )
{
	DPRINTK("audio_dma_pause - enter\n");
	
	ep93xx_dma_pause( stream->dmahandles[0],
					  stream->NumDmaChannels, 
					  stream->dmahandles );

	if (state->hw->hw_disable)
		state->hw->hw_disable( stream );
	
	if (state->hw->hw_clear_fifo)
		state->hw->hw_clear_fifo( stream );

	DPRINTK("audio_dma_pause - EXIT\n");
}

static void audio_prime_dma( audio_state_t * state, audio_stream_t * stream )
{
	int i;
	
	DPRINTK("audio_prime_dma\n");

	/*
	 * If we haven't already started the DMA start it.
	 * But don't start it if we are waiting on a trigger.
	 */
	if( !stream->active && !stream->stopped )
	{
		stream->active = 1;
		audio_dma_start( state, stream );
	}

	for (i = 0; i < stream->nbfrags; i++) 
	{
		audio_buf_t *dma_buffer = stream->dma_buffer;
		down(&dma_buffer->sem);
	 	ep93xx_dma_add_buffer( stream->dmahandles[0],		/* dma instance 		*/
							   (unsigned int)dma_buffer->dma_addr, /* source 		*/
							   0, 							/* dest 				*/
							   stream->fragsize,			/* size					*/
							   0,							/* is the last chunk? 	*/
							   (unsigned int) dma_buffer ); /* buf id			 	*/	
		NEXT_BUF(stream, dma_buffer);
	}
}

/*
 * audio_read
 *
 * Audio capture function.
 */
static int audio_read
(
	struct file *file, 
	char * user_buffer,
	size_t count, 
	loff_t * ppos
)
{
	char * user_buffer_start = user_buffer;
	audio_state_t * state = (audio_state_t *)file->private_data;
	audio_stream_t * stream = state->input_stream;
	unsigned int user_buffer_xfer_count = 0, dma_buffer_xfer_count = 0; 
	int ret = 0;

	DPRINTK("EP93xx - audio_read: count=%d\n", count);

	if (ppos != &file->f_pos)
		return -ESPIPE;
		
	if (stream->mapped)
		return -ENXIO;

	if( !access_ok( VERIFY_WRITE, user_buffer, count ) )
	{
		DPRINTK( "access_ok failed for audio_read!!!!\n" );
		return -EFAULT;
	}

	if (!stream->active) 
	{
		if (!stream->buffers && audio_allocate_buffers( state, stream))
			return -ENOMEM;

		audio_prime_dma( state, stream );
	}

	while (count > 0) 
	{
		/*
		 * Get the current buffer.
		 */
		audio_buf_t * dma_buffer = stream->dma_buffer;

		/* 
		 * Wait for a dma buffer to become full 
		 */
		if (file->f_flags & O_NONBLOCK) 
		{
			//DPRINTK("file->f_flags & O_NONBLOCK\n");
			ret = -EAGAIN;
			if (down_trylock(&dma_buffer->sem))
				break;
		} 
		else 
		{
			//DPRINTK("NOT file->f_flags & O_NONBLOCK\n");
			ret = -ERESTARTSYS;
			if (down_interruptible(&dma_buffer->sem))
				break;
		}

		/*
		 * If this is the first buffer captured,
		 * dump the first 8 samples of the dma buffer.
		 */
		if( stream->bFirstCaptureBuffer )
		{
			dma_buffer->size -= 64;
			stream->bFirstCaptureBuffer = 0;
		}	
		
		/*
		 * How much user buffer would a whole DMA buffer fill?
		 */
		user_buffer_xfer_count = dma_buffer->size / stream->dma2usr_ratio;

		if (user_buffer_xfer_count > count)
			user_buffer_xfer_count = count;
		
		dma_buffer_xfer_count = user_buffer_xfer_count * stream->dma2usr_ratio;

		DPRINTK("dma_buffer: start=0x%08x fragsz=%d size=%d\n",
				 (int)dma_buffer->start, (int)stream->fragsize, dma_buffer->size);
		
		DPRINTK("user_buffer=0x%08x  from=0x%08x  user_count=%d  dma_count=%d\n",
				 (int)user_buffer,
				 (int)dma_buffer->start + stream->fragsize - dma_buffer->size,
				 user_buffer_xfer_count, dma_buffer_xfer_count);
		
		
		if( copy_to_user_with_conversion( state, 
				 user_buffer,
				 dma_buffer->start + stream->fragsize - dma_buffer->size,
				 user_buffer_xfer_count) <= 0 ) 
		{
			up(&dma_buffer->sem);
			return -EFAULT;
		}
		
		dma_buffer->size -= dma_buffer_xfer_count;

		user_buffer += user_buffer_xfer_count;
		count -= user_buffer_xfer_count;
		
		/* 
		 * Grab data from the current dma buffer 
		 */
		//DPRINTK("Read: read %d bytes from %d.  dmabufsize=%d.  count=%d\n", 
		//			user_buffer_xfer_count, stream->dma_buffer_index,
		//			dma_buffer->size, count);
		
		/*
		 * If there's still data in this buffer to be read, release
		 * the semiphore and don't give it back yet.  We may come back
		 * and read from it in a minute when the app calls for another read.
		 */
		if (dma_buffer->size > 0) 
		{
		//	DPRINTK("dma_buffer->size > 0 !\n");
			up(&dma_buffer->sem);
			break;
		}

		/* Make current dma buffer available for DMA again */
	 	ep93xx_dma_add_buffer( stream->dmahandles[0],		/* dma instance  */
							   (unsigned int)dma_buffer->dma_addr, /* source */
							   0, 									/* dest  */
							   stream->fragsize,					/* size	 */
							   0,				  			/* the last chunk? */
							   (unsigned int) dma_buffer );        /* buf id */	

		NEXT_BUF(stream, dma_buffer);
	}

	if( (int)user_buffer - (int)user_buffer_start )
		ret = (int)user_buffer - (int)user_buffer_start;
	
	//DPRINTK("EP93xx - audio_read: return=%d\n", ret);
	
	return ret;
}


static int audio_mmap(struct file *file, struct vm_area_struct *vma)
{
	audio_state_t *state = (audio_state_t *)file->private_data;
	audio_stream_t * stream = 0;
	unsigned long size, vma_addr;
	int i, ret;

	if (vma->vm_pgoff != 0)
		return -EINVAL;

	if (vma->vm_flags & VM_WRITE) 
	{
		if (!state->wr_ref)
			return -EINVAL;

		stream = state->output_stream;
	}
	else if (vma->vm_flags & VM_READ) 
	{
		if (!state->rd_ref)
			return -EINVAL;

		stream = state->input_stream;
	} 
	else 
	{
		return -EINVAL;
	}

	if (stream->mapped)
		return -EINVAL;

	size = vma->vm_end - vma->vm_start;
	
	if (size != stream->fragsize * stream->nbfrags)
		return -EINVAL;
	
	if (!stream->buffers && audio_allocate_buffers( state, stream))
		return -ENOMEM;

	vma_addr = vma->vm_start;

	for (i = 0; i < stream->nbfrags; i++) 
	{
		audio_buf_t *dma_buffer = &stream->buffers[i];
		
		if (!dma_buffer->master)
		{
			continue;
		}

		ret = remap_page_range(vma_addr, dma_buffer->dma_addr, dma_buffer->master, vma->vm_page_prot);

		if (ret)
			return ret;

		vma_addr += dma_buffer->master;
	}

	stream->mapped = 1;

	return 0;
}


static unsigned int audio_poll(struct file *file,
			       struct poll_table_struct *wait)
{
	audio_state_t *state = (audio_state_t *)file->private_data;
	audio_stream_t *is = state->input_stream;
	audio_stream_t *os = state->output_stream;
	unsigned int mask = 0;
	int i;

	DPRINTK("EP93xx - audio_poll(): mode=%s%s\n",
		(file->f_mode & FMODE_READ) ? "r" : "",
		(file->f_mode & FMODE_WRITE) ? "w" : "");

	if (file->f_mode & FMODE_READ) 
	{
		/* Start audio input if not already active */
		if (!is->active) 
		{
			if (!is->buffers && audio_allocate_buffers( state, is))
				return -ENOMEM;

			audio_prime_dma( state, is);
		}

		poll_wait(file, &is->wq, wait);
	}

	if (file->f_mode & FMODE_WRITE) 
	{
		if (!os->buffers && audio_allocate_buffers( state, os))
			return -ENOMEM;

		poll_wait(file, &os->wq, wait);
	}

	if (file->f_mode & FMODE_READ) 
	{
		if (is->mapped) 
		{
			/* 
			 * if the buffer is mapped assume we care that there are 
			 * more bytes available than when we last asked using 
			 * SNDCTL_DSP_GETxPTR 
			 */
			if (is->bytecount != is->getptrCount)
				mask |= POLLIN | POLLRDNORM;
		} 
		else 
		{
			for (i = 0; i < is->nbfrags; i++) 
			{
				if (atomic_read(&is->buffers[i].sem.count) > 0) 
				{
					mask |= POLLIN | POLLRDNORM;
					break;
				}
			}
		}
	}
	
	if (file->f_mode & FMODE_WRITE) 
	{
		if (os->mapped) 
		{
			if (os->bytecount != os->getptrCount)
				mask |= POLLOUT | POLLWRNORM;
		} 
		else 
		{
			for (i = 0; i < os->nbfrags; i++) 
			{
				if (atomic_read(&os->buffers[i].sem.count) > 0) 
				{
					mask |= POLLOUT | POLLWRNORM;
					break;
				}
			}
		}
	}

	DPRINTK("EP93xx - audio_poll() returned mask of %s%s\n",
		(mask & POLLIN) ? "r" : "",
		(mask & POLLOUT) ? "w" : "");

	return mask;
}

/*
 * audio_set_fragments
 *
 * Used to process SNDCTL_DSP_SETFRAGMENT.
 *
 * Argument is 0xMMMMSSSS where:
 *		MMMM sets number of fragments.
 * 		SSSS sets dma fragment (dma_buffer) size.  size = 2^SSSS bytes
 */
static int audio_set_fragments( audio_state_t *state, audio_stream_t *stream, int val )
{
	if (stream->active)
		return -EBUSY;
	
	stream->requested_fragsize = 1 << (val & 0x000ffff);
	stream->requested_nbfrags  = (val >> 16) & 0x7FFF;
	
	return 0;
}

static void print_audio_format( long format )
{
	switch( format )
	{
		case AFMT_U8:		   
			DPRINTK_IOCTL( "AFMT_U8\n" );		   
			break;

		case AFMT_S8:
			DPRINTK_IOCTL( "AFMT_S8\n" );		   
			break;

		case AFMT_S32_BLOCKED:
			DPRINTK_IOCTL( "AFMT_S32_BLOCKED\n" );		   
			break;

		case AFMT_S16_LE:
			DPRINTK_IOCTL( "AFMT_S16_LE\n" );		   
			break;

		case AFMT_S16_BE:
			DPRINTK_IOCTL( "AFMT_S16_BE\n" );		   
			break;

		case AFMT_U16_LE:
			DPRINTK_IOCTL( "AFMT_U16_LE\n" );		   
			break;

		case AFMT_U16_BE:
		default:
			DPRINTK_IOCTL( "AFMT_U16_BE\n" );		   
			break;
	}
}

/*
 * We convert to 24 bit samples that occupy 32 bits each.
 * Formats we support:
 *
 * AFMT_U8		   
 * AFMT_S16_LE	   	Little endian signed 16
 * AFMT_S8		   
 * AFMT_U16_LE	   	Little endian U16
 * AFMT_S32_BLOCKED	32 bit little endian format, taken from the rme96xx driver.
 *
 */
static long audio_set_format( audio_stream_t * stream, long val )
{
	DPRINTK_IOCTL( "audio_set_format enter.  Format requested (%d) ", 
				(int)val );
	print_audio_format( val );
	
	switch( val )
	{
        case AFMT_QUERY:
            break;

		case AFMT_U8:		   
			stream->audio_format = val;
			stream->audio_stream_bitwidth = 8;
			break;

		case AFMT_S8:
			stream->audio_format = val;
			stream->audio_stream_bitwidth = 8;
			break;

		case AFMT_S32_BLOCKED:
			stream->audio_format = val;
			stream->audio_stream_bitwidth = 32;
			break;

		case AFMT_S16_LE:
		case AFMT_S16_BE:
			stream->audio_format = AFMT_S16_LE;
			stream->audio_stream_bitwidth = 16;
			break;

		case AFMT_U16_LE:
		case AFMT_U16_BE:
		default:
			stream->audio_format = AFMT_U16_LE;
			stream->audio_stream_bitwidth = 16;
			break;
	}

	DPRINTK_IOCTL( "audio_set_format EXIT format set to be (%d) ", 
				(int)stream->audio_format );
	print_audio_format( (long)stream->audio_format );
	
	return stream->audio_format;
}


static int audio_ioctl(struct inode *inode, struct file *file,
		       uint cmd, ulong arg)
{
	audio_state_t *state = (audio_state_t *)file->private_data;
	audio_stream_t *os = state->output_stream;
	audio_stream_t *is = state->input_stream;
	long val=0;
	
	switch (cmd) 
	{
		case SNDCTL_DSP_STEREO:
			DPRINTK_IOCTL("audio_ioctl - SNDCTL_DSP_STEREO\n");
			if( get_user(val, (int *)arg) || (val > 1) || (val < 0) )
				return -EFAULT;

			if (file->f_mode & FMODE_WRITE)
			{
				if (os->audio_num_channels != (val + 1)) {
					os->audio_num_channels = val + 1;
					calculate_dma2usr_ratio_etc( os );
				}
			} 

			if (file->f_mode & FMODE_READ) 
			{
				if (is->audio_num_channels != (val + 1)) {
					is->audio_num_channels = val + 1;
					calculate_dma2usr_ratio_etc( is );
				}
			} 

			return put_user( val, (long *) arg);

		case SNDCTL_DSP_CHANNELS:
			DPRINTK_IOCTL("audio_ioctl - SNDCTL_DSP_CHANNELS\n");
			if( get_user(val, (int *)arg) || (val > 2) || (val < 0) )
				return -EFAULT;

			// Calling with value of 0 is a query.
			if( val == 0 )
			{
				if (file->f_mode & FMODE_WRITE)
					val = os->audio_num_channels;
				else
					val = is->audio_num_channels;
			}
			else
			{
				if (file->f_mode & FMODE_WRITE)
				{
					if (os->audio_num_channels != val) {
						os->audio_num_channels = val;
						calculate_dma2usr_ratio_etc( os );
					} 
				} 
				if (file->f_mode & FMODE_READ) 
				{
					if (is->audio_num_channels != val) {
						is->audio_num_channels = val;
						calculate_dma2usr_ratio_etc( is );
					} 
				}
			}			
			 
			return put_user( val, (long *) arg);

		case SOUND_PCM_READ_CHANNELS:
			DPRINTK_IOCTL("audio_ioctl - SOUND_PCM_READ_CHANNELS\n");
			if (file->f_mode & FMODE_WRITE)
				val = os->audio_num_channels;
			else
				val = is->audio_num_channels;
			return put_user( val, (long *) arg);

		case SNDCTL_DSP_SETFMT:
		{
			DPRINTK_IOCTL("audio_ioctl - SNDCTL_DSP_SETFMT\n");
			
			if (get_user(val, (int *)arg))
				return -EFAULT;
			
			if( val != AFMT_QUERY )
			{
				if( (file->f_mode & FMODE_WRITE) && (val != os->audio_format) )
				{
					audio_set_format( os, val );
					calculate_dma2usr_ratio_etc( os );
					if( state->hw->set_hw_serial_format )
						state->hw->set_hw_serial_format( os, val );
				}
				if( (file->f_mode & FMODE_READ) && (val != is->audio_format) )
				{
					audio_set_format( is, val );
					calculate_dma2usr_ratio_etc( is );
					if( state->hw->set_hw_serial_format )
						state->hw->set_hw_serial_format( is, val );
				}
			}
			
			if (file->f_mode & FMODE_WRITE)
				return put_user( os->audio_format, (long *) arg);
			return put_user( is->audio_format, (long *) arg);
		}
		
		case SNDCTL_DSP_GETFMTS:
			DPRINTK_IOCTL("audio_ioctl - SNDCTL_DSP_GETFMTS\n");
			return put_user( (AFMT_U8 | AFMT_S8 | AFMT_S16_LE | AFMT_U16_LE), 
							 (long *) arg);

		case OSS_GETVERSION:
			DPRINTK_IOCTL("audio_ioctl - OSS_GETVERSION\n");
			return put_user(SOUND_VERSION, (int *)arg);

		case SNDCTL_DSP_GETBLKSIZE:
			DPRINTK_IOCTL("audio_ioctl - SNDCTL_DSP_GETBLKSIZE\n");
			if (file->f_mode & FMODE_WRITE)
				return put_user(os->fragsize, (int *)arg);
			return put_user(is->fragsize, (int *)arg);
			
		case SNDCTL_DSP_GETCAPS:
			DPRINTK_IOCTL("audio_ioctl - SNDCTL_DSP_GETCAPS\n");
		 	val = DSP_CAP_REALTIME|DSP_CAP_TRIGGER|DSP_CAP_MMAP;
			if (is && os)
				val |= DSP_CAP_DUPLEX;
			return put_user(val, (int *)arg);

		case SNDCTL_DSP_SETFRAGMENT:
		{
			int ret;
			DPRINTK_IOCTL("audio_ioctl - SNDCTL_DSP_SETFRAGMENT\n");
			if (get_user(val, (long *) arg))
				return -EFAULT;
			
			if (file->f_mode & FMODE_READ)
			{ 
				ret = audio_set_fragments(state, is, val);
				if (ret<0)
					return ret;
			}
			
			if (file->f_mode & FMODE_WRITE) 
			{ 
				ret = audio_set_fragments(state, os, val);
				if (ret<0)
					return ret;
			}

			return 0;
		}

		case SNDCTL_DSP_SYNC:
			DPRINTK_IOCTL("audio_ioctl - SNDCTL_DSP_SYNC\n");
			return audio_sync(file);

		case SNDCTL_DSP_SETDUPLEX:
			DPRINTK_IOCTL("audio_ioctl - SNDCTL_DSP_SETDUPLEX\n");
			return 0;

		case SNDCTL_DSP_POST:
			DPRINTK_IOCTL("audio_ioctl - SNDCTL_DSP_POST\n");
			return 0;

		case SNDCTL_DSP_GETTRIGGER:
			DPRINTK_IOCTL("audio_ioctl - SNDCTL_DSP_GETTRIGGER\n");
			val = 0;
			
			if (file->f_mode & FMODE_READ && is->active && !is->stopped)
				val |= PCM_ENABLE_INPUT;
				
			if (file->f_mode & FMODE_WRITE && os->active && !os->stopped)
				val |= PCM_ENABLE_OUTPUT;
				
			return put_user(val, (int *)arg);

		case SNDCTL_DSP_SETTRIGGER:
			DPRINTK_IOCTL("audio_ioctl - SNDCTL_DSP_SETTRIGGER\n");
			if (get_user(val, (int *)arg))
				return -EFAULT;
				
			if (file->f_mode & FMODE_READ) 
			{
				if (val & PCM_ENABLE_INPUT) 
				{
					if (!is->active) 
					{
						if (!is->buffers && audio_allocate_buffers( state, is))
							return -ENOMEM;
						audio_prime_dma( state, is);
					}
					
					if (is->stopped) 
					{
						is->stopped = 0;
						os->active = 1;
						audio_dma_start( state, is );
					}
				} 
				else 
				{
					audio_dma_pause( state, is );
					is->stopped = 1;
					is->active = 0;
				}
			}
			
			if (file->f_mode & FMODE_WRITE) 
			{
				if (val & PCM_ENABLE_OUTPUT) 
				{
					if (!os->active) 
					{
						if (!os->buffers && audio_allocate_buffers( state, os))
							return -ENOMEM;
							
						if (os->mapped)
							audio_prime_dma( state, os );
					}
					
					if (os->stopped) 
					{
						os->stopped = 0;
						os->active = 1;
						audio_dma_start( state, os );
					}
				} 
				else 
				{
					audio_dma_pause( state, os );
					os->stopped = 1;
					os->active = 0;
				}
			}
			return 0;

		case SNDCTL_DSP_GETOPTR:
		case SNDCTL_DSP_GETIPTR:
		{
			count_info inf = { 0, };
			audio_stream_t * stream = (cmd == SNDCTL_DSP_GETOPTR) ? os : is;
			int bytecount, fragcount, flags, buf_num=0;
			unsigned int current_frac, buf_id;
			
			DPRINTK_IOCTL("audio_ioctl - called SNDCTL_DSP_GETOPTR.\n");

			if ((stream == is && !(file->f_mode & FMODE_READ)) ||
			    (stream == os && !(file->f_mode & FMODE_WRITE)))
				return -EINVAL;
				
			if( !stream->active ) 
				return copy_to_user((void *)arg, &inf, sizeof(inf));
			
		   	save_flags_cli(flags);
			
			ep93xx_dma_get_position( stream->dmahandles[0], &buf_id, 0, 
			    &current_frac );

			/*
			 * Out of a set of nbfrags buffers, which one is the current one?
			 */
			if (buf_id)
		   		buf_num = ((audio_buf_t *)buf_id)->num;

			/*
			 * The ep93xx-audio ISRs keep track of bytecount and fragcount.
			 * This bytecount isn't cleared except when the driver is opened
			 * or when it overflows.
			 */
			bytecount = stream->bytecount;
			
			/* so poll can tell if it changes */
			stream->getptrCount = stream->bytecount;	

			/*
			 * Return the number of fragment transitions counted in our dma ISR
			 * since the previous call to this ioctl, then clear that counter.
			 */
			fragcount = stream->fragcount;
			stream->fragcount = 0;
			
			restore_flags(flags);

			/*
			 * When reporting position to user app, scale byte counts and the
			 * ptr to reflect the difference in sample size between dma samples
			 * and user samples.  But don't scale the # of fragments.
			 */
			inf.blocks = fragcount;
			inf.bytes  = (bytecount + current_frac) / stream->dma2usr_ratio;
			inf.ptr = ((buf_num * stream->fragsize) + current_frac) / stream->dma2usr_ratio;

			return copy_to_user((void *)arg, &inf, sizeof(inf));
		}
			
		case SNDCTL_DSP_GETODELAY:
		{
			int flags, buffered_bytes_left, count;
			unsigned int current_frac;

			if (!(file->f_mode & FMODE_WRITE))
				return -EINVAL;

			DPRINTK_IOCTL("audio_ioctl - called SNDCTL_DSP_GETODELAY.\n");

		   	save_flags_cli(flags);
			
			ep93xx_dma_get_position( os->dmahandles[0], 0, 0, &current_frac );

			buffered_bytes_left = os->buffered_bytes_to_play;
			
			restore_flags(flags);
			
			count = (buffered_bytes_left - current_frac) / os->dma2usr_ratio;

			return put_user(count, (int *)arg);
		}

		case SNDCTL_DSP_GETOSPACE:
		    {
				/*
				 * It looks like this code is assuming that nobody
				 * is writing the device while calling this ioctl
				 * which may be a reasonable assumption.  I hope.
				 * Maybe.
				 */
				audio_buf_info inf = { 0, };
				int i;

				DPRINTK_IOCTL("audio_ioctl - SNDCTL_DSP_GETOSPACE\n");

				if (!(file->f_mode & FMODE_WRITE))
					return -EINVAL;
				if (!os->buffers && audio_allocate_buffers( state, os))
					return -ENOMEM;
					
				for (i = 0; i < os->nbfrags; i++) 
				{
					/*
					 * If the semaphore is 1, the buffer is available.
					 */
					if (atomic_read(&os->buffers[i].sem.count) > 0) 
					{
						if (os->buffers[i].size == 0)
							inf.fragments++;
						
						inf.bytes += os->fragsize - os->buffers[i].size;
					}
				}
				
				inf.fragsize = os->fragsize / os->dma2usr_ratio;
				inf.fragstotal = os->nbfrags;
				inf.bytes = inf.bytes / os->dma2usr_ratio;
			
				return copy_to_user((void *)arg, &inf, sizeof(inf));
		    }

		case SNDCTL_DSP_GETISPACE:
		    {
				audio_buf_info inf = { 0, };
				int i;

			 	DPRINTK_IOCTL("audio_ioctl - SNDCTL_DSP_GETISPACE\n");

				if (!(file->f_mode & FMODE_READ))
					return -EINVAL;
				if (!is->buffers && audio_allocate_buffers( state, is))
					return -ENOMEM;
					
				for (i = 0; i < is->nbfrags; i++) 
				{
					if (atomic_read(&is->buffers[i].sem.count) > 0) 
					{
						if (is->buffers[i].size == is->fragsize)
							inf.fragments++;
						
						inf.bytes += is->buffers[i].size;
					}
				}
				
				inf.fragsize = is->fragsize / is->dma2usr_ratio;
				inf.fragstotal = is->nbfrags;
				inf.bytes = inf.bytes / is->dma2usr_ratio;

				return copy_to_user((void *)arg, &inf, sizeof(inf));
		    }

		case SNDCTL_DSP_NONBLOCK:
			DPRINTK_IOCTL("audio_ioctl - SNDCTL_DSP_NONBLOCK\n");
			file->f_flags |= O_NONBLOCK;
			return 0;

		case SNDCTL_DSP_RESET:
			DPRINTK_IOCTL("audio_ioctl - SNDCTL_DSP_RESET\n");
			if (file->f_mode & FMODE_READ) 
				audio_reset_buffers(state,is);
			if (file->f_mode & FMODE_WRITE) 
				audio_reset_buffers(state,os);
			return 0;

		default:
			/*
			 * Let the client of this module handle the
			 * non generic ioctls
			 */
			return state->hw->client_ioctl(inode, file, cmd, arg);
	}

	return 0;
}


static int audio_release(struct inode *inode, struct file *file)
{
	audio_state_t *state = (audio_state_t *)file->private_data;
	int i;

	DPRINTK("EP93xx - audio_release -- enter.  Write=%d  Read=%d\n",
			((file->f_mode & FMODE_WRITE) != 0),
			((file->f_mode & FMODE_READ) != 0)	);

	down(&state->sem);

	if (file->f_mode & FMODE_READ) 
	{
		audio_deallocate_buffers(state, state->input_stream);
		
		for( i=0 ; i < state->input_stream->NumDmaChannels ; i++ )
			ep93xx_dma_free( state->input_stream->dmahandles[i] );

		state->rd_ref = 0;
	}

	if (file->f_mode & FMODE_WRITE) 
	{
		/*
		 * Wait for all our buffers to play.
		 */
		audio_sync(file);
		
		/*
		 * audio_deallocate_buffers will pause and flush the dma
		 * then deallocate the buffers.
		 */
		audio_deallocate_buffers(state, state->output_stream);
		for( i=0 ; i < state->output_stream->NumDmaChannels ; i++ )
			ep93xx_dma_free( state->output_stream->dmahandles[i] );

		state->wr_ref = 0;
	}

	up(&state->sem);

	DPRINTK("EP93xx - audio_release -- EXIT\n");

	return 0;
}

/*
 *  ep93xx_clear_stream_struct
 *
 *  Initialize stuff or at least clear it to zero for a stream structure.
 */
static void ep93xx_clear_stream_struct( audio_stream_t * stream )
{
	
	stream->dmahandles[0] 		= 0;	/* handles for dma driver instances */
	stream->dmahandles[1] 		= 0;	/* handles for dma driver instances */
	stream->dmahandles[2] 		= 0;	/* handles for dma driver instances */
	stream->NumDmaChannels		= 0;	/* 1, 2, or 3 DMA channels 			*/
	stream->TX_dma_ch_bitfield	= 0;	/* which dma channels used for strm */
	stream->RX_dma_ch_bitfield	= 0;	/* which dma channels used for strm */
	stream->buffers				= 0;	/* array of audio buffer structures */
	stream->dma_buffer			= 0;	/* current buffer used by read/write */
	stream->dma_buffer_index	= 0;	/* index for the pointer above... 	*/
	stream->fragsize			= AUDIO_FRAGSIZE_DEFAULT;	/* fragment i.e. buffer size 		*/
	stream->nbfrags				= AUDIO_NBFRAGS_DEFAULT;	/* nbr of fragments i.e. buffers 	*/
	stream->requested_fragsize	= 0;	/* This is ignored if 0 */
	stream->requested_nbfrags	= 0;	/* This is ignored if 0 */
	stream->bytecount			= 0;	/* nbr of processed bytes 			*/
	stream->getptrCount			= 0;	/* value of bytecount last time 	*/
	 							  		/* anyone asked via GETxPTR 		*/
	stream->fragcount			= 0;	/* nbr of fragment transitions 		*/
	stream->mapped				= 0;	/* mmap()'ed buffers 				*/
	stream->active				= 0;	/* actually in progress 			*/
	stream->stopped				= 0;	/* might be active but stopped 		*/
	stream->bFirstCaptureBuffer = 1;	/* flag meaning there is junk in rx fifo */
	stream->buffered_bytes_to_play = 0;
	
	calculate_dma2usr_ratio_etc( stream );
}

/*
 *	ep93xx_audio_attach
 *
 *  Initialize the state structure for read or write.  Allocate dma channels.
 *  Currently each s is a stereo s (one dma channel).
 *
 */
int ep93xx_audio_attach
(
	struct inode *inode, 
	struct file *file,
	audio_state_t *state
)
{
	int err, i;
	audio_stream_t *os = state->output_stream;
	audio_stream_t *is = state->input_stream;

	DPRINTK("ep93xx_audio_attach -- enter.  Write=%d  Read=%d\n",
			((file->f_mode & FMODE_WRITE) != 0),
			((file->f_mode & FMODE_READ) != 0)	);

	down(&state->sem);

	/* access control */
	if( ((file->f_mode & FMODE_WRITE) && !os) ||
	    ((file->f_mode & FMODE_READ) && !is) )
	{
		up(&state->sem);
		return -ENODEV;
	}

	if( ((file->f_mode & FMODE_WRITE) && state->wr_ref) ||
	    ((file->f_mode & FMODE_READ) && state->rd_ref) )
	{
		up(&state->sem);
		return -EBUSY;
	}

	/* 
	 * Request DMA channels 
	 */
	if( file->f_mode & FMODE_WRITE ) 
	{
		DPRINTK("ep93xx_audio_attach -- FMODE_WRITE\n");
		
		state->wr_ref = 1;

		ep93xx_clear_stream_struct( os );
	
		err = ep93xx_dma_request( 	&os->dmahandles[0], 
									os->devicename,
                       				os->dmachannel[0] );
		if (err)
		{
			up(&state->sem);
			DPRINTK("ep93xx_audio_attach -- EXIT ERROR dma request failed\n");
			return err;
		}

		err = ep93xx_dma_config( 	os->dmahandles[0], 
									IGNORE_CHANNEL_ERROR, 
									0,
									audio_dma_tx_callback, 
									(unsigned int)state );
		if (err) 
		{
			ep93xx_dma_free( os->dmahandles[0] );
			up(&state->sem);
			DPRINTK("ep93xx_audio_attach -- EXIT ERROR dma config failed\n");
			return err;
		}

		/*
		 * Assuming each audio stream is mono or stereo.
		 */
		os->NumDmaChannels = 1;

		/*
		 * Automatically set up the bit field of dma channels used for
		 * this stream.
		 */
		for( i=0 ; ( i < state->hw->MaxTxDmaChannels ) ; i++ )
		{
			if( os->dmachannel[i] == state->hw->txdmachannels[0] )
				os->TX_dma_ch_bitfield |= 1;	

			if( os->dmachannel[i] == state->hw->txdmachannels[1] )
				os->TX_dma_ch_bitfield |= 2;	
			
			if( os->dmachannel[i] == state->hw->txdmachannels[2] )
				os->TX_dma_ch_bitfield |= 4;	
		}
		
		DPRINTK("DMA Tx channel bitfield = %x\n", os->TX_dma_ch_bitfield );

		if (state->hw->hw_clear_fifo)
			state->hw->hw_clear_fifo( os );

		init_waitqueue_head(&os->wq);
	}
	
	if( file->f_mode & FMODE_READ ) 
	{
		state->rd_ref = 1;
		
		ep93xx_clear_stream_struct( is );

		err = ep93xx_dma_request( 	&is->dmahandles[0], 
									is->devicename,
                       				is->dmachannel[0] );
		if (err) 
		{
			up(&state->sem);
			DPRINTK("ep93xx_audio_attach -- EXIT ERROR dma request failed\n");
			return err;
		}

		err = ep93xx_dma_config( 	is->dmahandles[0], 
									IGNORE_CHANNEL_ERROR, 
									0,
									audio_dma_rx_callback, 
									(unsigned int)state );
		if (err) 
		{
			ep93xx_dma_free( is->dmahandles[0] );
			up(&state->sem);
			DPRINTK("ep93xx_audio_attach -- EXIT ERROR dma config failed\n");
			return err;
		}

		is->NumDmaChannels = 1;

		/*
		 * Automatically set up the bit field of dma channels used for
		 * this stream.
		 */
		for( i=0 ; ( i < state->hw->MaxRxDmaChannels ) ; i++ )
		{
			if( is->dmachannel[i] == state->hw->rxdmachannels[0] )
				is->RX_dma_ch_bitfield |= 1;	

			if( is->dmachannel[i] == state->hw->rxdmachannels[1] )
				is->RX_dma_ch_bitfield |= 2;	

			if( is->dmachannel[i] == state->hw->rxdmachannels[2] )
				is->RX_dma_ch_bitfield |= 4;	
		}

		DPRINTK("DMA Rx channel bitfield = %x\n", is->RX_dma_ch_bitfield );

		init_waitqueue_head(&is->wq);
	}

	/*
	 * Fill out the rest of the file_operations struct.
	 */
	file->private_data	= state;
	file->f_op->release	= audio_release;
	file->f_op->write	= audio_write;
	file->f_op->read	= audio_read;
	file->f_op->mmap	= audio_mmap;
	file->f_op->poll	= audio_poll;
	file->f_op->ioctl	= audio_ioctl;
	file->f_op->llseek	= no_llseek;

	up(&state->sem);

	DPRINTK("ep93xx_audio_attach -- EXIT\n");

	return 0;
}

EXPORT_SYMBOL(ep93xx_audio_attach);

MODULE_DESCRIPTION("Common audio handling for the Cirrus EP93xx processor");
MODULE_LICENSE("GPL");
