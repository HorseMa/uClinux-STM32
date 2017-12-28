/*
 * Glue audio driver for the Cirrus EP93xx Ac97 Controller
 *
 * Copyright (c) 2003 Cirrus Logic Corp.
 * Copyright (c) 2000 Nicolas Pitre <nico@cam.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License.
 *
 * This file taken from h3600-uda1341.c
 * The mixer code is from ac97_codec.c
 *
 * History:
 *
 * 2000-05-21   Nicolas Pitre   Initial UDA1341 driver release.
 *
 * 2000-07-??   George France   Bitsy support.
 *
 * 2000-12-13   Deborah Wallach Fixed power handling for iPAQ/h3600
 *
 * 2001-06-03   Nicolas Pitre   Made this file a separate module, based on
 *              the former sa1100-uda1341.c driver.
 *
 * 2001-07-13   Nicolas Pitre   Fixes for all supported samplerates.
 *
 * 2003-04-04   adt				Changes for Cirrus Logic EP93xx I2S
 *
 * 2003-04-25   adt				Changes for Cirrus Logic EP93xx Ac97
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/errno.h>
#include <linux/sound.h>
#include <linux/soundcard.h>
#include <linux/bitops.h>

#include <asm/semaphore.h>
#include <asm/uaccess.h>
#include <asm/hardware.h>
#include <asm/dma.h>
#include <asm/io.h>

#include "ep93xx-audio.h"


#undef DEBUG
//#define DEBUG 1
#ifdef DEBUG
#define DPRINTK( fmt, arg... )  printk( fmt, ##arg )
#else
#define DPRINTK( fmt, arg... )
#endif


#define AUDIO_NAME      "ep93xx_ac97"

#define AUDIO_SAMPLE_RATE_DEFAULT   44100

/* original check is not good enough in case FOO is greater than
 * SOUND_MIXER_NRDEVICES because the supported_mixers has exactly
 * SOUND_MIXER_NRDEVICES elements.
 * before matching the given mixer against the bitmask in supported_mixers we
 * check if mixer number exceeds maximum allowed size which is as mentioned
 * above SOUND_MIXER_NRDEVICES */
#define supported_mixer(FOO) \
	( (FOO >= 0) && \
	(FOO < SOUND_MIXER_NRDEVICES) && \
	codec_supported_mixers & (1<<FOO) )

/*
 * Available record sources.  
 * LINE1 refers to AUX in.  
 * IGAIN refers to input gain which means stereo mix.
 */
#define AC97_RECORD_MASK \
	(SOUND_MASK_MIC | SOUND_MASK_CD | SOUND_MASK_IGAIN | SOUND_MASK_VIDEO |\
	SOUND_MASK_LINE1 | SOUND_MASK_LINE | SOUND_MASK_PHONEIN)

#define AC97_STEREO_MASK \
	(SOUND_MASK_VOLUME | SOUND_MASK_PCM | SOUND_MASK_LINE | SOUND_MASK_CD | \
	SOUND_MASK_ALTPCM | SOUND_MASK_IGAIN | SOUND_MASK_LINE1 | SOUND_MASK_VIDEO)

#define AC97_SUPPORTED_MASK \
	(AC97_STEREO_MASK | SOUND_MASK_BASS | SOUND_MASK_TREBLE | \
	SOUND_MASK_SPEAKER | SOUND_MASK_MIC | \
	SOUND_MASK_PHONEIN | SOUND_MASK_PHONEOUT)


/* 
 * Function prototypes.
 */
static int 		peek(unsigned int uiAddress);
static int 		poke(unsigned int uiAddress, unsigned int uiValue);
static void 	ep93xx_setup_src(void);
static void 	ep93xx_set_samplerate( audio_stream_t * stream, long lSampleRate );
static void 	ep93xx_init_ac97_codec( void );
static void 	ep93xx_set_hw_format( audio_stream_t * stream, long format );
static void	 	ep93xx_init_ac97_controller( void );
static int 		ep93xx_mixer_ioctl(struct inode *inode, struct file *file, uint cmd, ulong arg);
static void 	ep93xx_audio_init(void *dummy);
static int 		ep93xx_audio_ioctl(struct inode *inode, struct file *file,
				 uint cmd, ulong arg);
static int 		ep93xx_audio_open(struct inode *inode, struct file *file);
static int __init ep93xx_ac97_init(void);
static void __exit ep93xx_ac97_exit(void);
static void 	ep93xx_init_mixer(void);
static void		ep93xx_audio_enable( audio_stream_t * stream );
static void		ep93xx_audio_disable( audio_stream_t * stream );

/*
 * Statics
 */
static int ac_link_enabled = 0;
static int codec_supported_mixers;

/* this table has default mixer values for all OSS mixers. */
typedef struct  {
	int mixer;
	unsigned int value;
} mixer_defaults_t;

/*
 * Default mixer settings that are set up during boot.
 *
 * These values are 16 bit numbers in which the upper byte is right volume
 * and the lower byte is left volume or mono volume for mono controls.
 *
 * OSS Range for each of left and right volumes is 0 to 100 (0x00 to 0x64).
 * 
 */
static mixer_defaults_t mixer_defaults[SOUND_MIXER_NRDEVICES] = 
{
	/* Outputs */
	{SOUND_MIXER_VOLUME,	0x6464},   /* 0 dB */  /* -46.5dB to  0 dB */
	{SOUND_MIXER_ALTPCM,	0x6464},   /* 0 dB */  /* -46.5dB to  0 dB */
	{SOUND_MIXER_PHONEOUT,	0x6464},   /* 0 dB */  /* -46.5dB to  0 dB */

	/* PCM playback gain */
	{SOUND_MIXER_PCM,		0x4b4b},   /* 0 dB */  /* -34.5dB to +12dB */

	/* Record gain */
	{SOUND_MIXER_IGAIN,		0x0000},   /* 0 dB */  /*    0 to +22.5 dB */

	/* Inputs */
	{SOUND_MIXER_MIC,		0x0000},   /* mute */  /* -34.5dB to +12dB */
	{SOUND_MIXER_LINE,		0x4b4b},   /* 0 dB */  /* -34.5dB to +12dB */

	/* Inputs that are not connected. */
	{SOUND_MIXER_SPEAKER,	0x0000},   /* mute */  /* -45dB   to   0dB */
	{SOUND_MIXER_PHONEIN,	0x0000},   /* mute */  /* -34.5dB to +12dB */
	{SOUND_MIXER_CD,		0x0000},   /* mute */  /* -34.5dB to +12dB */
	{SOUND_MIXER_VIDEO,		0x0000},   /* mute */  /* -34.5dB to +12dB */
	{SOUND_MIXER_LINE1,		0x0000},   /* mute */  /* -34.5dB to +12dB */

	{-1,0} /* last entry */
};

static unsigned int guiOSS_Volume[SOUND_MIXER_NRDEVICES];

/* table to scale scale from OSS mixer value to AC97 mixer register value */	
typedef struct {
	unsigned int offset;
	int scale;
} ac97_mixer_hw_t; 

static ac97_mixer_hw_t ac97_hw[SOUND_MIXER_NRDEVICES] = 
{
	[SOUND_MIXER_VOLUME]		=  	{AC97_02_MASTER_VOL,	64},
	[SOUND_MIXER_BASS]			=	{0, 0},
	[SOUND_MIXER_TREBLE]		=	{0, 0},
	[SOUND_MIXER_SYNTH]			=  	{0,	0},
	[SOUND_MIXER_PCM]			=  	{AC97_18_PCM_OUT_VOL,	32},
	[SOUND_MIXER_SPEAKER]		=  	{AC97_0A_PC_BEEP_VOL,	32},
	[SOUND_MIXER_LINE]			=  	{AC97_10_LINE_IN_VOL,	32},
	[SOUND_MIXER_MIC]			=  	{AC97_0E_MIC_VOL,		32},
	[SOUND_MIXER_CD]			=  	{AC97_12_CD_VOL,		32},
	[SOUND_MIXER_IMIX]			=  	{0,	0},
	[SOUND_MIXER_ALTPCM]		=  	{AC97_04_HEADPHONE_VOL,	64},
	[SOUND_MIXER_RECLEV]		=  	{0,	0},
	[SOUND_MIXER_IGAIN]			=  	{AC97_1C_RECORD_GAIN,	16},
	[SOUND_MIXER_OGAIN]			=  	{0,	0},
	[SOUND_MIXER_LINE1]			=  	{AC97_16_AUX_VOL,		32},
	[SOUND_MIXER_LINE2]			=  	{0,	0},
	[SOUND_MIXER_LINE3]			=  	{0,	0},
	[SOUND_MIXER_DIGITAL1]		=  	{0,	0},
	[SOUND_MIXER_DIGITAL2]		=  	{0,	0},
	[SOUND_MIXER_DIGITAL3]		=  	{0,	0},
	[SOUND_MIXER_PHONEIN]		=  	{AC97_0C_PHONE_VOL,		32},
	[SOUND_MIXER_PHONEOUT]		=  	{AC97_06_MONO_VOL,		64},
	[SOUND_MIXER_VIDEO]			=  	{AC97_14_VIDEO_VOL,		32},
	[SOUND_MIXER_RADIO]			=  	{0,	0},
	[SOUND_MIXER_MONITOR]		=  	{0,	0},
};


/* the following tables allow us to go from OSS <-> ac97 quickly. */
enum ac97_recsettings 
{
	AC97_REC_MIC=0,
	AC97_REC_CD,
	AC97_REC_VIDEO,
	AC97_REC_AUX,
	AC97_REC_LINE,
	AC97_REC_STEREO, /* combination of all enabled outputs..  */
	AC97_REC_MONO,	      /*.. or the mono equivalent */
	AC97_REC_PHONE
};

static const unsigned int ac97_rm2oss[] = 
{
	[AC97_REC_MIC] 	 = SOUND_MIXER_MIC,
	[AC97_REC_CD] 	 = SOUND_MIXER_CD,
	[AC97_REC_VIDEO] = SOUND_MIXER_VIDEO,
	[AC97_REC_AUX] 	 = SOUND_MIXER_LINE1,
	[AC97_REC_LINE]  = SOUND_MIXER_LINE,
	[AC97_REC_STEREO]= SOUND_MIXER_IGAIN,
	[AC97_REC_PHONE] = SOUND_MIXER_PHONEIN
};

/* indexed by bit position */
static const unsigned int ac97_oss_rm[] = 
{
	[SOUND_MIXER_MIC] 	= AC97_REC_MIC,
	[SOUND_MIXER_CD] 	= AC97_REC_CD,
	[SOUND_MIXER_VIDEO] = AC97_REC_VIDEO,
	[SOUND_MIXER_LINE1] = AC97_REC_AUX,
	[SOUND_MIXER_LINE] 	= AC97_REC_LINE,
	[SOUND_MIXER_IGAIN]	= AC97_REC_STEREO,
	[SOUND_MIXER_PHONEIN] 	= AC97_REC_PHONE
};

static audio_stream_t ac97_output_stream0 =
{
	audio_num_channels: 	EP93XX_DEFAULT_NUM_CHANNELS,
	audio_format: 			EP93XX_DEFAULT_FORMAT,
	audio_stream_bitwidth: 	EP93XX_DEFAULT_BIT_WIDTH,
	devicename:          	"Ac97_out",
	dmachannel:				{ DMATx_AAC1, UNDEF, UNDEF },
	sample_rate:			0,
	hw_bit_width:			16,
	bCompactMode:			1,
};

static audio_stream_t ac97_input_stream0 =
{
	audio_num_channels: 	EP93XX_DEFAULT_NUM_CHANNELS,
	audio_format: 			EP93XX_DEFAULT_FORMAT,
	audio_stream_bitwidth: 	EP93XX_DEFAULT_BIT_WIDTH,
	devicename:           	"Ac97_in",
	dmachannel:				{ DMARx_AAC1, UNDEF, UNDEF },
	sample_rate:			0,
	hw_bit_width:			16,
	bCompactMode:			1,
};

static audio_hw_t ep93xx_ac97_hw =
{
	hw_enable:          ep93xx_audio_enable,
	hw_disable:         ep93xx_audio_disable,
	hw_clear_fifo:		0,
	client_ioctl:       ep93xx_audio_ioctl,
	
	set_hw_serial_format:	ep93xx_set_hw_format,

	txdmachannels:		{ DMATx_AAC1, DMATx_AAC2, DMATx_AAC3 },
	rxdmachannels:		{ DMARx_AAC1, DMARx_AAC2, DMARx_AAC3 },
	
	MaxTxDmaChannels:	3,
	MaxRxDmaChannels:	3,
	
	modcnt:				0,
};

static audio_state_t ac97_audio_state0 =
{
	output_stream:      &ac97_output_stream0,
	input_stream:       &ac97_input_stream0,
	sem:                __MUTEX_INITIALIZER(ac97_audio_state0.sem),
	hw:					&ep93xx_ac97_hw,
	wr_ref:				0,
	rd_ref:				0,
};


/*
 * peek
 *
 * Reads an AC97 codec register.  Returns -1 if there was an error.
 */
static int peek(unsigned int uiAddress)
{
	unsigned int uiAC97RGIS, uiTemp;
	
	if( !ac_link_enabled )
	{
		DPRINTK("ep93xx ac97 peek: attempt to peek before enabling ac-link.\n");
		return -1;
	}
	
	/*
	 * Check to make sure that the address is aligned on a word boundary 
	 * and is 7E or less.
	 */
	if( ((uiAddress & 0x1)!=0) || (uiAddress > 0x007e))
	{
		return -1;
	}

	/*
	 * How it is supposed to work is:
	 *  - The ac97 controller sends out a read addr in slot 1.
	 *  - In the next frame, the codec will echo that address back in slot 1
	 *    and send the data in slot 2.  SLOT2RXVALID will be set to 1.
	 *
	 * Read until SLOT2RXVALID goes to 1.  Reading the data in AC97S2DATA
	 * clears SLOT2RXVALID.
	 */

	/*
	 * First, delay one frame in case of back to back peeks/pokes.
	 */
	mdelay( 1 );

	/*
	 * Write the address to AC97S1DATA, delay 1 frame, read the flags.
	 */
	outl( uiAddress, AC97S1DATA);
	uiTemp = inl(AC97GCR); /* read to push write out the wrapper */

	udelay( 21 * 4 );
	uiAC97RGIS = inl( AC97RGIS );

	/*
	 * Return error if we timed out.
	 */
	if( ((uiAC97RGIS & AC97RGIS_SLOT1TXCOMPLETE) == 0 ) &&
		((uiAC97RGIS & AC97RGIS_SLOT2RXVALID) == 0 ) )
	{
		DPRINTK( "ep93xx-ac97 - peek failed reading reg 0x%02x.\n", uiAddress ); 
		return -1;
	}
	
	return ( inl(AC97S2DATA) & 0x000fffff);
}

/*
 * poke
 *
 * Writes an AC97 codec Register.  Return -1 if error.
 */
static int poke(unsigned int uiAddress, unsigned int uiValue)
{
	unsigned int uiAC97RGIS, uiTemp;

	if( !ac_link_enabled )
	{
		DPRINTK("ep93xx ac97 poke: attempt to poke before enabling ac-link.\n");
		return -1;
	}
	
	/*
	 * Check to make sure that the address is align on a word boundary and
	 * is 7E or less.  And that the value is a 16 bit value.
	 */
	if( ((uiAddress & 0x1)!=0) || (uiAddress > 0x007e))
	{
		return -1;
	}
	
	/*
	 * First, delay one frame in case of back to back peeks/pokes.
	 */
	mdelay( 1 );

	/*
	 * Write the data to AC97S2DATA, then the address to AC97S1DATA.
	 */
	outl( uiValue, AC97S2DATA );
	outl( uiAddress, AC97S1DATA );
	uiTemp = inl(AC97GCR); /* read to push writes out the wrapper */
	
	/*
	 * Wait for the tx to complete, get status.
	 */
	udelay( 30 );
	uiAC97RGIS = inl(AC97RGIS);

	/*
	 * Return error if we timed out.
	 */
	if( !(inl(AC97RGIS) & AC97RGIS_SLOT1TXCOMPLETE) )
	{
		DPRINTK( "ep93xx-ac97: poke failed writing reg 0x%02x  value 0x%02x.\n", uiAddress, uiValue ); 
		return -1;
	}

	return 0;
}

/*
 * When we get to the multichannel case the pre-fill and enable code
 * will go to the dma driver's start routine.
 */
static void ep93xx_audio_enable( audio_stream_t * stream )
{
	unsigned int uiTemp;

	DPRINTK("ep93xx_audio_enable\n");

	/*
	 * Enable the rx or tx channel
	 */
	if( stream->dmachannel[0] == DMATx_AAC1 )
	{
		uiTemp = inl(AC97TXCR1);
		outl( (uiTemp | AC97TXCR_TEN), AC97TXCR1 );
		uiTemp = inl(AC97TXCR1);
	}
	
	if( stream->dmachannel[0] == DMARx_AAC1 )
	{
		uiTemp = inl(AC97RXCR1);
		outl( (uiTemp | AC97RXCR_REN), AC97RXCR1 );
		uiTemp = inl(AC97RXCR1);
	}

	//DPRINTK("ep93xx_audio_enable - EXIT\n");
}

static void ep93xx_audio_disable( audio_stream_t * stream )
{
	unsigned int uiControl, uiStatus;

	DPRINTK("ep93xx_audio_disable\n");

	/*
	 * Disable the rx or tx channel
	 */
	if( stream->dmachannel[0] == DMATx_AAC1 )
	{
		uiControl = inl(AC97TXCR1);
		if( uiControl & AC97TXCR_TEN )
		{
			/* 
			 * Wait for fifo to empty.  We've shut down the dma
			 * so that should happen soon.
			 */
			do {
				uiStatus = inl( AC97SR1 );
			} while( ((uiStatus & 0x82)==0) );
			//} while( ((uiStatus & 0x20)!=0) ;&&
					 //((uiStatus & 0x82)==0) );
			
			outl( (uiControl & ~AC97TXCR_TEN), AC97TXCR1 );
			uiControl = inl(AC97TXCR1);
		}
	}

	if( stream->dmachannel[0] == DMARx_AAC1 )
	{
		uiControl = inl(AC97RXCR1);
		outl( (uiControl & ~AC97RXCR_REN), AC97RXCR1 );
		uiControl = inl(AC97RXCR1);
	}

	//DPRINTK("ep93xx_audio_disable - EXIT\n");
}

/*
 * ep93xx_setup_src
 *
 * Once the ac-link is up and all is good, we want to set the codec to a 
 * usable mode.
 */
static void ep93xx_setup_src(void)
{
	int iTemp;

	/*
	 * Set the VRA bit to enable the SRC.
	 */
	iTemp = peek( AC97_2A_EXT_AUDIO_POWER );
	poke( AC97_2A_EXT_AUDIO_POWER,  (iTemp | 0x1) );
	
	/*
	 * Set the DSRC/ASRC bits to enable the variable rate SRC.
	 */
	iTemp = peek( AC97_60_MISC_CRYSTAL_CONTROL  );
	poke( AC97_60_MISC_CRYSTAL_CONTROL, (iTemp  | 0x0300) );
}

/*
 * ep93xx_set_samplerate
 *
 *   lFrequency		- Sample Rate in Hz
 *   bTx			- 1 to set Tx sample rate
 *   bRx			- 1 to set Rx sample rate
 */
static void ep93xx_set_samplerate( audio_stream_t * stream, long lSampleRate )
{
	unsigned short usDivider, usPhase;

	DPRINTK( "ep93xx_set_samplerate - Fs = %d\n", (int)lSampleRate );

	if( (lSampleRate <  7200) || (lSampleRate > 48000)  )
	{
		DPRINTK( "ep93xx_set_samplerate - invalid Fs = %d\n", 
				 (int)lSampleRate );
		return;
	}

	/*
	 * Calculate divider and phase increment.
	 *
	 * divider = round( 0x1770000 / lSampleRate )
	 *  Note that usually rounding is done by adding 0.5 to a floating 
	 *  value and then truncating.  To do this without using floating
	 *  point, I multiply the fraction by two, do the division, then add one, 
	 *  then divide the whole by 2 and then truncate.
	 *  Same effect, no floating point math.
	 *
	 * Ph incr = trunc( (0x1000000 / usDivider) + 1 )
	 */

#ifdef CONFIG_MACH_IPD
	/* we have a different codec */
	usPhase = 0;
	if (stream->dmachannel[0] == DMATx_AAC1)
		poke(AC97_2C_PCM_FRONT_DAC_RATE, lSampleRate);
	else
		poke(AC97_32_PCM_LR_ADC_RATE, lSampleRate);
#else
	usDivider = (unsigned short)( ((2 * 0x1770000 / lSampleRate) +  1) / 2 );

	usPhase = (0x1000000 / usDivider) + 1;

	/*
	 * Write them in the registers.  Spec says divider must be
	 * written after phase incr.
	 */
	if( stream->dmachannel[0] == DMATx_AAC1 )
	{
		poke( AC97_2C_PCM_FRONT_DAC_RATE, usDivider);
		poke( AC97_64_DAC_SRC_PHASE_INCR, usPhase);
	}

	if( stream->dmachannel[0] == DMARx_AAC1 )
	{
		
		poke( AC97_32_PCM_LR_ADC_RATE,  usDivider);
		poke( AC97_66_ADC_SRC_PHASE_INCR, usPhase);
	}
#endif
	
	DPRINTK( "ep93xx_set_samplerate - phase = %d,  divider = %d\n",
				(unsigned int)usPhase, (unsigned int)usDivider );

	/*
	 * We sorta should report the actual samplerate back to the calling
	 * application.  But some applications freak out if they don't get
	 * exactly what they asked for.  So we fudge and tell them what
	 * they want to hear.
	 */
	stream->sample_rate = lSampleRate;

	DPRINTK( "ep93xx_set_samplerate - EXIT\n" );
}

/*
 * ep93xx_set_hw_format
 *
 * Sets up whether the controller is expecting 20 bit data in 32 bit words
 * or 16 bit data compacted to have a stereo sample in each 32 bit word.
 */
static void ep93xx_set_hw_format( audio_stream_t * stream, long format )
{
	int bCompactMode, uiTemp, iWidth;
	unsigned long ulRegValue;
	
	switch( format )
	{
		/*
		 * Here's all the <=16 bit formats.  We can squeeze both L and R
		 * into one 32 bit sample so use compact mode.
		 */
		case AFMT_U8:		   
		case AFMT_S8:		   
		case AFMT_S16_LE:
		case AFMT_U16_LE:
			bCompactMode = 1;
			ulRegValue = 0x00008018;
			iWidth = 16;
			break;

		/*
		 * Add any other >16 bit formats here...
		 */
		case AFMT_S32_BLOCKED:
		default:
			bCompactMode = 0;
			ulRegValue = 0x00004018;
			iWidth = 20;
			break;
	}
	
	if( stream->dmachannel[0] == DMARx_AAC1 )
	{
		uiTemp = inl( AC97RXCR1 );
		if( ulRegValue != uiTemp )
		{
			outl( ulRegValue, AC97RXCR1 );
			uiTemp = inl( AC97RXCR1 );
		} 
		stream->hw_bit_width = iWidth;
		stream->bCompactMode = bCompactMode;
	}
	
	if( stream->dmachannel[0] == DMATx_AAC1 )
	{
		uiTemp = inl( AC97TXCR1 );
		if( ulRegValue != uiTemp )
		{
			outl( ulRegValue, AC97TXCR1 );
			uiTemp = inl( AC97TXCR1 );
		}
		stream->hw_bit_width = iWidth;
		stream->bCompactMode = bCompactMode;
	}
	
}

/*
 * ep93xx_init_ac97_controller
 *
 * This routine sets up the Ac'97 Controller.
 */
static void ep93xx_init_ac97_controller(void)
{
	unsigned int uiDEVCFG, uiTemp;

	DPRINTK("ep93xx_init_ac97_controller - enter\n");

	/*
	 * Configure the multiplexed Ac'97 pins to be Ac97 not I2s.
	 * Configure the EGPIO4 and EGPIO6 to be GPIOS, not to be  
	 * SDOUT's for the second and third I2S controller channels.
	 */
	uiDEVCFG = inl(SYSCON_DEVCFG);
	
	uiDEVCFG &= ~(SYSCON_DEVCFG_I2SonAC97 | 
				  SYSCON_DEVCFG_A1onG |
				  SYSCON_DEVCFG_A2onG);
		
	SysconSetLocked(SYSCON_DEVCFG, uiDEVCFG);

	/*
	 * Disable the AC97 controller internal loopback.  
	 * Disable Override codec ready.
	 */
	outl( 0, AC97GCR );

	/*
	 * Enable the AC97 Link.
	 */
	uiTemp = inl(AC97GCR);
	outl( (uiTemp | AC97GSR_IFE), AC97GCR );
	uiTemp = inl(AC97GCR); /* read to push write out the wrapper */

	/*
	 * Set the TIMEDRESET bit.  Will cause a > 1uSec reset of the ac-link.
	 * This bit is self resetting.
	 */
	outl( AC97RESET_TIMEDRESET, AC97RESET );
	uiTemp = inl(AC97GCR); /* read to push write out the wrapper */
	
	/*
	 *  Delay briefly, but let's not hog the processor.
	 */
	set_current_state(TASK_INTERRUPTIBLE);
	schedule_timeout( 5 ); /* 50 mSec */

	/*
	 * Read the AC97 status register to see if we've seen a CODECREADY
	 * signal from the AC97 codec.
	 */
	if( !(inl(AC97RGIS) & AC97RGIS_CODECREADY))
	{
		DPRINTK( "ep93xx-ac97 - FAIL: CODECREADY still low!\n");
		return;
	}

	/*
	 *  Delay for a second, not hogging the processor
	 */
	set_current_state(TASK_INTERRUPTIBLE);
	schedule_timeout( HZ ); /* 1 Sec */
	
	/*
	 * Now the Ac-link is up.  We can read and write codec registers.
	 */
	ac_link_enabled = 1;

	/*
	 * Set up the rx and tx channels
	 * Set the CM bit, data size=16 bits, enable tx slots 3 & 4.
	 */
	ep93xx_set_hw_format( &ac97_output_stream0, EP93XX_DEFAULT_FORMAT );
	ep93xx_set_hw_format( &ac97_input_stream0, EP93XX_DEFAULT_FORMAT );

	DPRINTK( "ep93xx-ac97 -- AC97RXCR1:  %08x\n", inl(AC97RXCR1) ); 
	DPRINTK( "ep93xx-ac97 -- AC97TXCR1:  %08x\n", inl(AC97TXCR1) ); 

	DPRINTK("ep93xx_init_ac97_controller - EXIT - success\n");

}

/*
 * ep93xx_init_ac97_codec
 *
 * Program up the external Ac97 codec.
 *
 */
static void ep93xx_init_ac97_codec( void )
{
	DPRINTK("ep93xx_init_ac97_codec - enter\n");

	ep93xx_setup_src();
	ep93xx_set_samplerate( &ac97_output_stream0, AUDIO_SAMPLE_RATE_DEFAULT );
	ep93xx_set_samplerate( &ac97_input_stream0,  AUDIO_SAMPLE_RATE_DEFAULT );
	ep93xx_init_mixer();

	DPRINTK("ep93xx_init_ac97_codec - EXIT\n");
	
}

#ifdef DEBUG
static void ep93xx_dump_ac97_regs(void)
{
	int i;
	unsigned int reg0, reg1, reg2, reg3, reg4, reg5, reg6, reg7;
	
	DPRINTK( "---------------------------------------------\n");
	DPRINTK( "   :   0    2    4    6    8    A    C    E\n" ); 
	
	for( i=0 ; i < 0x80 ; i+=0x10 )
	{
		reg0 = 0xffff & (unsigned int)peek( i );
		reg1 = 0xffff & (unsigned int)peek( i + 0x2 );
		reg2 = 0xffff & (unsigned int)peek( i + 0x4 );
		reg3 = 0xffff & (unsigned int)peek( i + 0x6 );
		reg4 = 0xffff & (unsigned int)peek( i + 0x8 );
		reg5 = 0xffff & (unsigned int)peek( i + 0xa );
		reg6 = 0xffff & (unsigned int)peek( i + 0xc );
		reg7 = 0xffff & (unsigned int)peek( i + 0xe );

		DPRINTK( " %02x : %04x %04x %04x %04x %04x %04x %04x %04x\n", 
				 i, reg0, reg1, reg2, reg3, reg4, reg5, reg6, reg7);
	}
}
#endif

static void ep93xx_set_volume
(
	unsigned int oss_channel, 
	unsigned int oss_val 
) 
{
	unsigned int left,right;
	u16 val = 0;
	ac97_mixer_hw_t * mh = &ac97_hw[oss_channel];

	if( !mh->scale )
	{
		DPRINTK( "ep93xx-ac97.c: ep93xx_set_volume - not a valid OSS channel\n");
		return;
	}

	/* cleanse input a little */
	right = ((oss_val >> 8)  & 0xff) ;
	left = (oss_val  & 0xff) ;

	if (right > 100) right = 100;
	if (left > 100) left = 100;

	DPRINTK("ac97_codec: wrote OSS channel#%2d (ac97 reg 0x%02x), "
	       "l:%2d, r:%2d:",
	       oss_channel, mh->offset, left, right);

	if( AC97_STEREO_MASK & (1 << oss_channel) ) 
	{
		/* stereo mixers */
		if (oss_channel == SOUND_MIXER_IGAIN) 
		{
			right = ((100 - right) * (mh->scale - 1)) / 100;
			left =  ((100 - left)  * (mh->scale - 1)) / 100;
			if (right >= mh->scale)
				right = mh->scale-1;
			if (left >= mh->scale)
				left = mh->scale-1;
			right = (mh->scale - 1) - right;
			left  = (mh->scale - 1) - left;
			val = (left << 8) | right;
		}
		else 
		{
			if (left == 0 && right == 0) 
			{
				val = 0x8000;
			} 
			else
			{
				right = ((100 - right) * (mh->scale - 1)) / 100;
				left =  ((100 - left)  * (mh->scale - 1)) / 100;
				if (right >= mh->scale)
					right = mh->scale-1;
				if (left >= mh->scale)
					left = mh->scale-1;
				val = (left << 8) | right;
			}
		}
	} 
	else if(left == 0) 
	{
		val = 0x8000;
	} 
	else if( (oss_channel == SOUND_MIXER_SPEAKER) ||
			(oss_channel == SOUND_MIXER_PHONEIN) ||
			(oss_channel == SOUND_MIXER_PHONEOUT) )
	{
		left = ((100 - left) * (mh->scale - 1)) / 100;
		if (left >= mh->scale)
			left = mh->scale-1;
		val = left;
	} 
	else if (oss_channel == SOUND_MIXER_MIC) 
	{
		val = peek( mh->offset) & ~0x801f;
		left = ((100 - left) * (mh->scale - 1)) / 100;
		if (left >= mh->scale)
			left = mh->scale-1;
		val |= left;
	} 
	/*  
	 * For bass and treble, the low bit is optional.  Masking it
	 * lets us avoid the 0xf 'bypass'.
	 * Do a read, modify, write as we have two contols in one reg. 
	 */
	else if (oss_channel == SOUND_MIXER_BASS) 
	{
		val = peek( mh->offset) & ~0x0f00;
		left = ((100 - left) * (mh->scale - 1)) / 100;
		if (left >= mh->scale)
			left = mh->scale-1;
		val |= (left << 8) & 0x0e00;
	} 
	else if (oss_channel == SOUND_MIXER_TREBLE) 
	{
		val = peek( mh->offset) & ~0x000f;
		left = ((100 - left) * (mh->scale - 1)) / 100;
		if (left >= mh->scale)
			left = mh->scale-1;
		val |= left & 0x000e;
	}
	
	DPRINTK(" 0x%04x", val);

	poke( mh->offset, val );

#ifdef DEBUG
	val = peek( mh->offset );
	DPRINTK(" (read back 0x%04x)\n", val);
#endif

	guiOSS_Volume[oss_channel] = oss_val;
}

static void ep93xx_init_mixer(void)
{
	u16 cap;
	int i;

	/* mixer masks */
	codec_supported_mixers 	= AC97_SUPPORTED_MASK;
	
	cap = peek( AC97_00_RESET );
	if( !(cap & 0x04) )
	{
		codec_supported_mixers &= ~(SOUND_MASK_BASS|SOUND_MASK_TREBLE);
	}
	if( !(cap & 0x10) )
	{
		codec_supported_mixers &= ~SOUND_MASK_ALTPCM;
	}

	/* 
	 * Detect bit resolution of output volume controls by writing to the
	 * 6th bit (not unmuting yet)
	 */
	poke( AC97_02_MASTER_VOL, 0xa020 );
	if( peek( AC97_02_MASTER_VOL) != 0xa020 )
	{
		ac97_hw[SOUND_MIXER_VOLUME].scale = 32;
	}

	poke( AC97_04_HEADPHONE_VOL, 0xa020 );
	if( peek( AC97_04_HEADPHONE_VOL) != 0xa020 )
	{
		ac97_hw[SOUND_MIXER_ALTPCM].scale = 32;
	}

	poke( AC97_06_MONO_VOL, 0x8020 );
	if( peek( AC97_06_MONO_VOL) != 0x8020 )
	{
		ac97_hw[SOUND_MIXER_PHONEOUT].scale = 32;
	}

	/* initialize mixer channel volumes */
	for( i = 0; 
		(i < SOUND_MIXER_NRDEVICES) && (mixer_defaults[i].mixer != -1) ; 
		i++ ) 
	{
		if( !supported_mixer( mixer_defaults[i].mixer) )
		{ 
			continue;
		}
		
		ep93xx_set_volume( mixer_defaults[i].mixer, mixer_defaults[i].value);
	}

}

/*
 * ac97_recmask_io
 *
 * Read or write the record source.
 */
static int ep93xx_read_recsource(void) 
{
	unsigned int val;

	/* read it from the card */
	val = peek( AC97_1A_RECORD_SELECT );
	
	DPRINTK("ac97_codec: ac97 recmask to set to 0x%04x\n", val);
	
	return( 1 << ac97_rm2oss[val & 0x07] );
}

static int ep93xx_set_recsource( int mask ) 
{
	unsigned int val;

	/* Arg contains a bit for each recording source */
	if( mask == 0 ) 
	{
		return 0;
	}
	
	mask &= AC97_RECORD_MASK;
	
	if( mask == 0 ) 
	{
		return -EINVAL;
	}
				
	/*
	 * May have more than one bit set.  So clear out currently selected
	 * record source value first (AC97 supports only 1 input) 
	 */
	val = (1 << ac97_rm2oss[peek( AC97_1A_RECORD_SELECT ) & 0x07]);
	if (mask != val)
	    mask &= ~val;
       
	val = ffs(mask); 
	val = ac97_oss_rm[val-1];
	val |= val << 8;  /* set both channels */

	DPRINTK("ac97_codec: setting ac97 recmask to 0x%04x\n", val);

	poke( AC97_1A_RECORD_SELECT, val);

	return 0;
}

static int 
ep93xx_mixer_open(struct inode *inode, struct file *file)
{
	file->private_data = &ep93xx_ac97_hw;
	return 0;
}

static int 
ep93xx_mixer_release(struct inode *inode, struct file *file)
{
	return 0;
}

static int
ep93xx_mixer_ioctl(struct inode *inode, struct file *file, uint cmd, ulong arg)
{
	int val, nr;

	audio_hw_t * hw = (audio_hw_t *)file->private_data;

	DPRINTK("ep93xx_mixer_ioctl - enter.  IOC_TYPE is %c \n", _IOC_TYPE(cmd) );

	if (cmd == SOUND_MIXER_INFO) 
	{
		mixer_info info;
		strncpy(info.id, "CS4202", sizeof(info.id));
		strncpy(info.name, "Cirrus CS4202", sizeof(info.name));
		info.modify_counter = hw->modcnt;
		if (copy_to_user((void *)arg, &info, sizeof(info)))
			return -EFAULT;
		return 0;
	}
	if( cmd == SOUND_OLD_MIXER_INFO ) 
	{
		_old_mixer_info info;
		strncpy(info.id, "CS4202", sizeof(info.id));
		strncpy(info.name, "Cirrus CS4202", sizeof(info.name));
		if (copy_to_user((void *)arg, &info, sizeof(info)))
			return -EFAULT;
		return 0;
	}

	if( (_IOC_TYPE(cmd) != 'M') || (_SIOC_SIZE(cmd) != sizeof(int)) )
		return -EINVAL;
	
	if( cmd == OSS_GETVERSION )
		return put_user(SOUND_VERSION, (int *)arg);
	
	nr = _IOC_NR(cmd);

	if( _SIOC_DIR(cmd) == _SIOC_READ ) 
	{
		switch( nr ) 
		{
			case SOUND_MIXER_RECSRC: 
				/* Read the current record source */
				val = ep93xx_read_recsource();
				break;

			case SOUND_MIXER_DEVMASK: 
				/* give them the supported mixers */
				val = codec_supported_mixers;
				break;

			case SOUND_MIXER_RECMASK: 
				/* Arg contains a bit for each supported recording source */
				val = AC97_RECORD_MASK;
				break;

			case SOUND_MIXER_STEREODEVS: 
				/* Mixer channels supporting stereo */
				val = AC97_STEREO_MASK;
				break;

			case SOUND_MIXER_CAPS:
				val = SOUND_CAP_EXCL_INPUT;
				break;

			default: 
				if( !supported_mixer( nr ) )
					return -EINVAL;
				
			    val = guiOSS_Volume[nr];
	 			break;
		
		} /* switch */
		
		return put_user(val, (int *)arg);
	}

	if( _SIOC_DIR(cmd) == (_SIOC_WRITE|_SIOC_READ) ) 
	{
		if (get_user(val, (int *)arg))
			return -EFAULT;
		
		switch( nr ) 
		{
			case SOUND_MIXER_RECSRC: 
				return ep93xx_set_recsource( val );
			
			default: 
				if( !supported_mixer( nr ) )
					return -EINVAL;
				
				ep93xx_set_volume( nr, val );
				hw->modcnt++;
				return 0;
		}
	}
	
	return -EINVAL;
}

static struct file_operations h3600_mixer_fops = 
{
	owner:		THIS_MODULE,
	llseek:		no_llseek,
	ioctl:		ep93xx_mixer_ioctl,
	open:		ep93xx_mixer_open,
	release:	ep93xx_mixer_release,
};


/*
 * Audio interface
 */
static void ep93xx_audio_init(void *dummy)
{
	DPRINTK("ep93xx_audio_init - enter\n");
	
	/*
	 * Init the controller, enable the ac-link.
	 * Initialize the codec.
	 */
	ep93xx_init_ac97_controller();
	ep93xx_init_ac97_codec();
	
#ifdef DEBUG
	ep93xx_dump_ac97_regs();
#endif

	DPRINTK("ep93xx_audio_init - EXIT\n");
}

static int ep93xx_audio_ioctl(struct inode *inode, struct file *file,
				 uint cmd, ulong arg)
{
	audio_state_t *state = (audio_state_t *)file->private_data;
	audio_stream_t *os = state->output_stream;
	audio_stream_t *is = state->input_stream;
	long val;
	int ret = 0;

	/*
	 * These are platform dependent ioctls which are not handled by the
	 * generic ep93xx-audio module.
	 */
	switch (cmd) 
	{
		case SNDCTL_DSP_SPEED:
			
			DPRINTK("ep93xx_audio_ioctl - SNDCTL_DSP_SPEED\n");
			
			if (get_user(val, (int *)arg))
			{
				return -EFAULT;
			}
			
			if( (file->f_mode & FMODE_WRITE) &&
				(val != os->sample_rate) )
			{
				ep93xx_set_samplerate( os, val );
			}
			if( (file->f_mode & FMODE_READ) &&
				(val != is->sample_rate) )
			{
				ep93xx_set_samplerate( is, val );
			}
			
			return put_user( val, (long *) arg );

		case SOUND_PCM_READ_RATE:
			DPRINTK("ep93xx_audio_ioctl - SOUND_PCM_READ_RATE\n");
			if( file->f_mode & FMODE_WRITE )
			{
				return put_user( os->sample_rate, (long *) arg);
			}
			return put_user( is->sample_rate, (long *) arg);

		default:
			DPRINTK("ep93xx_audio_ioctl-->ep93xx_mixer_ioctl\n");
			/* Maybe this is meant for the mixer (As per OSS Docs) */
			return ep93xx_mixer_ioctl(inode, file, cmd, arg);
	}

	return ret;
}

static int ep93xx_audio_open(struct inode *inode, struct file *file)
{
	DPRINTK( "ep93xx_audio_open\n" );

#ifdef CONFIG_MACH_IPD
{
	static int initted = 0;
	if (!initted) {
		/* set GPIO0/1 on to enable audio amp left&right*/
		poke(0x78, 0xabba);
		poke(0x74, peek(0x74) & ~0x0001);
		poke(0x3e, 0x81);
		poke(0x4c, 0x00);
		poke(0x4e, 0x00);
		poke(0x50, 0x03);
		poke(0x52, 0x00);
		poke(0x54, 0x03);
		poke(0x3e, 0x01);
		initted = 1;
	}
}
#endif

	return ep93xx_audio_attach( inode, file, &ac97_audio_state0 );
}

/*
 * Missing fields of this structure will be patched with the call
 * to ep93xx_audio_attach().
 */
static struct file_operations h3600_audio_fops = 
{
	open:       ep93xx_audio_open,
	owner:      THIS_MODULE
};


static int audio_dev_id, mixer_dev_id;

static int __init ep93xx_ac97_init(void)
{
	DPRINTK("ep93xx_ac97_init - enter\n");
	
	/*
	 * Enable audio early on, give the DAC time to come up.
	 */ 
	ep93xx_audio_init( 0 );

	/* 
	 * Register devices using sound_core.c's devfs stuff
	 */
	audio_dev_id = register_sound_dsp(&h3600_audio_fops, -1);
	if( audio_dev_id < 0 )
	{
		DPRINTK(" ep93xx_ac97_init: register_sound_dsp failed for dsp.\n" );
		return -ENODEV;
	}
	
	mixer_dev_id = register_sound_mixer(&h3600_mixer_fops, -1);
	if( mixer_dev_id < 0 )
	{
		DPRINTK(" ep93xx_ac97_init: register_sound_dsp failed for mixer.\n" );
		return -ENODEV;
	}

	printk( KERN_INFO "EP93xx Ac97 audio support initialized.\n" );
	return 0;
}

static void __exit ep93xx_ac97_exit(void)
{
	unregister_sound_dsp( audio_dev_id );
	unregister_sound_mixer( mixer_dev_id );
}

module_init(ep93xx_ac97_init);
module_exit(ep93xx_ac97_exit);

MODULE_DESCRIPTION("Audio driver for the Cirrus EP93xx Ac97 controller.");

/* EXPORT_NO_SYMBOLS; */
