/*
 * Glue audio driver for the Cirrus EP93xx I2S Controller
 *
 * Copyright (c) 2003 Cirrus Logic Corp.
 * Copyright (c) 2000 Nicolas Pitre <nico@cam.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License.
 *
 * This file taken from h3600-uda1341.c
 *
 * History:
 *
 * 2000-05-21	Nicolas Pitre	Initial UDA1341 driver release.
 *
 * 2000-07-??	George France	Bitsy support.
 *
 * 2000-12-13	Deborah Wallach Fixed power handling for iPAQ/h3600
 *
 * 2001-06-03	Nicolas Pitre	Made this file a separate module, based on
 *				the former sa1100-uda1341.c driver.
 *
 * 2001-07-13	Nicolas Pitre	Fixes for all supported samplerates.
 *
 * 2003-04-04   Changes for Cirrus Logic EP93xx
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

#include <asm/semaphore.h>
#include <asm/uaccess.h>
#include <asm/hardware.h>
#include <asm/dma.h>
#include <asm/arch/ssp.h>
#include <asm/io.h>

#include "ep93xx-audio.h"


#undef DEBUG
// #define DEBUG 1
#ifdef DEBUG
#define DPRINTK( fmt, arg... )  printk( fmt, ##arg )
#else
#define DPRINTK( fmt, arg... )
#endif


#define AUDIO_SAMPLE_RATE_DEFAULT	44100

/*
 * OSS mixer volume levels are 16 bit with two 8 bit fields for left and right.
 * Each is in % of full.  So range is 0..100 for each of two channels.
 */ 
#define AUDIO_DEFAULT_VOLUME ( (100<<8) | 100 )

/*
 * Statics
 */
static int giOSS_Volume[3] = 
	{AUDIO_DEFAULT_VOLUME,AUDIO_DEFAULT_VOLUME,AUDIO_DEFAULT_VOLUME};
static int giDAC_Volume_L[3] = {0,0,0};
static int giDAC_Volume_R[3] = {0,0,0};
static int giDAC_Mute = 0;

static int audio_clocks_enabled_in_syscon = 0;
static int SSP_Handle = -1;
static int audio_dev_id0, mixer_dev_id;
#ifdef CONFIG_CODEC_CS4228A
static int audio_dev_id1, audio_dev_id2;
#endif

/* prototypes */
static void ep93xx_audio_enable_all(void);
static void ep93xx_audio_disable_all(void);
static void ep93xx_clear_i2s_fifo( audio_stream_t * stream );
static int ep93xx_calc_closest_freq
(
    ulong   ulPLLFreq, 
    ulong   ulRequestedMClkFreq,
    ulong * pulActualMClkFreq,
    ulong * pulI2SDiv
);
static void ep93xx_set_samplerate(long lFrequency);
static void ep93xx_init_i2s_controller( void );

static void ep93xx_init_i2s_codec( void );
// static void ep93xx_mute_i2s_codec( void );
static void ep93xx_automute_i2s_codec( void );
static int ep93xx_set_volume( int channel, int val );

static int ep93xx_mixer_ioctl(struct inode *inode, struct file *file, uint cmd, ulong arg);

static void ep93xx_audio_init(void *dummy);
static int ep93xx_audio_ioctl(struct inode *inode, struct file *file,
			     uint cmd, ulong arg);
				 
static int ep93xx_audio_open0(struct inode *inode, struct file *file);
#ifdef CONFIG_CODEC_CS4228A
static int ep93xx_audio_open1(struct inode *inode, struct file *file);
static int ep93xx_audio_open2(struct inode *inode, struct file *file);
#endif

static int __init ep93xx_i2s_init(void);
static void __exit ep93xx_i2s_exit(void);

static audio_stream_t i2s_output_stream0 =
{
	audio_num_channels: 	EP93XX_DEFAULT_NUM_CHANNELS,
	audio_format: 			EP93XX_DEFAULT_FORMAT,
	audio_stream_bitwidth: 	EP93XX_DEFAULT_BIT_WIDTH,
	devicename:				"I2S_out_0",
	dmachannel:				{ DMATx_I2S1, UNDEF, UNDEF },
	sample_rate:			0,
	hw_bit_width:			24,
	bCompactMode:			0,
};

#if defined(CONFIG_ARCH_EDB9312) || defined(CONFIG_ARCH_EDB9315)
static audio_stream_t i2s_output_stream1 =
{
	audio_num_channels: 	EP93XX_DEFAULT_NUM_CHANNELS,
	audio_format: 			EP93XX_DEFAULT_FORMAT,
	audio_stream_bitwidth: 	EP93XX_DEFAULT_BIT_WIDTH,
	devicename:				"I2S_out_1",
	dmachannel:				{ DMATx_I2S2, UNDEF, UNDEF },
	sample_rate:			0,
	hw_bit_width:			24,
	bCompactMode:			0,
};
static audio_stream_t i2s_output_stream2 =
{
	audio_num_channels: 	EP93XX_DEFAULT_NUM_CHANNELS,
	audio_format: 			EP93XX_DEFAULT_FORMAT,
	audio_stream_bitwidth: 	EP93XX_DEFAULT_BIT_WIDTH,
	devicename:				"I2S_out_2",
	dmachannel:				{ DMATx_I2S3, UNDEF, UNDEF },
	sample_rate:			0,
	hw_bit_width:			24,
	bCompactMode:			0,
};
#endif

static audio_stream_t i2s_input_stream0 =
{
	audio_num_channels: 	EP93XX_DEFAULT_NUM_CHANNELS,
	audio_format: 			EP93XX_DEFAULT_FORMAT,
	audio_stream_bitwidth: 	EP93XX_DEFAULT_BIT_WIDTH,
	devicename:				"I2S_in_0",
	dmachannel:				{ DMARx_I2S1, UNDEF, UNDEF },
	sample_rate:			0,
	hw_bit_width:			24,
	bCompactMode:			0,
};

/*
 * The EDB9315, EDB9312, and EDB9301 boards all have one I2S input.
 * If you want more inputs and you have the ADC's for it, enable
 * this code.
 */
#if 0
static audio_stream_t i2s_input_stream1 =
{
	audio_num_channels: 	EP93XX_DEFAULT_NUM_CHANNELS,
	audio_format: 			EP93XX_DEFAULT_FORMAT,
	audio_stream_bitwidth: 	EP93XX_DEFAULT_BIT_WIDTH,
	devicename:				"I2S_in_1",
	dmachannel:				{ DMARx_I2S2, UNDEF, UNDEF },
	sample_rate:			0,
	hw_bit_width:			24,
	bCompactMode:			0,
};
static audio_stream_t i2s_input_stream2 =
{
	audio_num_channels: 	EP93XX_DEFAULT_NUM_CHANNELS,
	audio_format: 			EP93XX_DEFAULT_FORMAT,
	audio_stream_bitwidth: 	EP93XX_DEFAULT_BIT_WIDTH,
	devicename:				"I2S_in_2",
	dmachannel:				{ DMARx_I2S3, UNDEF, UNDEF },
	sample_rate:			0,
	hw_bit_width:			24,
	bCompactMode:			0,
};
#endif

static audio_hw_t ep93xx_i2s_hw =
{
	client_ioctl:		ep93xx_audio_ioctl,
	hw_enable:          0,
	hw_disable:         0,
	hw_clear_fifo:		ep93xx_clear_i2s_fifo,
	
	set_hw_serial_format:  0,

	txdmachannels:		{ DMATx_I2S1, DMATx_I2S2, DMATx_I2S3 },
	rxdmachannels:		{ DMARx_I2S1, DMARx_I2S2, DMARx_I2S3 },
	
	MaxTxDmaChannels:	3,
	MaxRxDmaChannels:	3,

	modcnt:				0,
};

/*
 * Each /dev/dsp* device is a 'state' with one input and one output stream.
 */
static audio_state_t audio_state0 =
{
	output_stream:		&i2s_output_stream0,
	input_stream:		&i2s_input_stream0,
	sem:				__MUTEX_INITIALIZER(audio_state0.sem),
	hw:					&ep93xx_i2s_hw,
	wr_ref:				0,
	rd_ref:				0,
};

#if CONFIG_CODEC_CS4228A
static audio_state_t audio_state1 =
{
	output_stream:		&i2s_output_stream1,
	input_stream:		0, /* &i2s_input_stream1, */
	sem:				__MUTEX_INITIALIZER(audio_state1.sem),
	hw:					&ep93xx_i2s_hw,
	wr_ref:				0,
	rd_ref:				0,
};
static audio_state_t audio_state2 =
{
	output_stream:		&i2s_output_stream2,
	input_stream:		0, /* &i2s_input_stream2, */
	sem:				__MUTEX_INITIALIZER(audio_state2.sem),
	hw:					&ep93xx_i2s_hw,
	wr_ref:				0,
	rd_ref:				0,
};
#endif


typedef struct {
    ulong   ulTotalDiv;
    ulong   ulI2SDiv;
} DIV_TABLE;

static const DIV_TABLE I2SDivTable[] =
{
    {   6, SYSCON_I2SDIV_PDIV_2  | (  2 & SYSCON_I2SDIV_MDIV_MASK) },  /* Fake entry for lower limit. */
    {   8, SYSCON_I2SDIV_PDIV_2  | (  2 & SYSCON_I2SDIV_MDIV_MASK) },
    {  10, SYSCON_I2SDIV_PDIV_25 | (  2 & SYSCON_I2SDIV_MDIV_MASK) },
    {  12, SYSCON_I2SDIV_PDIV_3  | (  2 & SYSCON_I2SDIV_MDIV_MASK) },
    {  15, SYSCON_I2SDIV_PDIV_25 | (  3 & SYSCON_I2SDIV_MDIV_MASK) },
    {  16, SYSCON_I2SDIV_PDIV_2  | (  4 & SYSCON_I2SDIV_MDIV_MASK) },
    {  18, SYSCON_I2SDIV_PDIV_3  | (  3 & SYSCON_I2SDIV_MDIV_MASK) },
    {  20, SYSCON_I2SDIV_PDIV_25 | (  4 & SYSCON_I2SDIV_MDIV_MASK) },
    {  24, SYSCON_I2SDIV_PDIV_3  | (  4 & SYSCON_I2SDIV_MDIV_MASK) },
    {  25, SYSCON_I2SDIV_PDIV_25 | (  5 & SYSCON_I2SDIV_MDIV_MASK) },
    {  28, SYSCON_I2SDIV_PDIV_2  | (  7 & SYSCON_I2SDIV_MDIV_MASK) },
    {  30, SYSCON_I2SDIV_PDIV_3  | (  5 & SYSCON_I2SDIV_MDIV_MASK) },
    {  32, SYSCON_I2SDIV_PDIV_2  | (  8 & SYSCON_I2SDIV_MDIV_MASK) },
    {  35, SYSCON_I2SDIV_PDIV_25 | (  7 & SYSCON_I2SDIV_MDIV_MASK) },
    {  36, SYSCON_I2SDIV_PDIV_3  | (  6 & SYSCON_I2SDIV_MDIV_MASK) },
    {  40, SYSCON_I2SDIV_PDIV_25 | (  8 & SYSCON_I2SDIV_MDIV_MASK) },
    {  42, SYSCON_I2SDIV_PDIV_3  | (  7 & SYSCON_I2SDIV_MDIV_MASK) },
    {  44, SYSCON_I2SDIV_PDIV_2  | ( 11 & SYSCON_I2SDIV_MDIV_MASK) },
    {  45, SYSCON_I2SDIV_PDIV_25 | (  9 & SYSCON_I2SDIV_MDIV_MASK) },
    {  48, SYSCON_I2SDIV_PDIV_3  | (  8 & SYSCON_I2SDIV_MDIV_MASK) },
    {  50, SYSCON_I2SDIV_PDIV_25 | ( 10 & SYSCON_I2SDIV_MDIV_MASK) },
    {  52, SYSCON_I2SDIV_PDIV_2  | ( 13 & SYSCON_I2SDIV_MDIV_MASK) },
    {  54, SYSCON_I2SDIV_PDIV_3  | (  9 & SYSCON_I2SDIV_MDIV_MASK) },
    {  55, SYSCON_I2SDIV_PDIV_25 | ( 11 & SYSCON_I2SDIV_MDIV_MASK) },
    {  56, SYSCON_I2SDIV_PDIV_2  | ( 14 & SYSCON_I2SDIV_MDIV_MASK) },
    {  60, SYSCON_I2SDIV_PDIV_3  | ( 10 & SYSCON_I2SDIV_MDIV_MASK) },
    {  64, SYSCON_I2SDIV_PDIV_2  | ( 16 & SYSCON_I2SDIV_MDIV_MASK) },
    {  65, SYSCON_I2SDIV_PDIV_25 | ( 13 & SYSCON_I2SDIV_MDIV_MASK) },
    {  66, SYSCON_I2SDIV_PDIV_3  | ( 11 & SYSCON_I2SDIV_MDIV_MASK) },
    {  68, SYSCON_I2SDIV_PDIV_2  | ( 17 & SYSCON_I2SDIV_MDIV_MASK) },
    {  70, SYSCON_I2SDIV_PDIV_25 | ( 14 & SYSCON_I2SDIV_MDIV_MASK) },
    {  72, SYSCON_I2SDIV_PDIV_3  | ( 12 & SYSCON_I2SDIV_MDIV_MASK) },
    {  75, SYSCON_I2SDIV_PDIV_25 | ( 15 & SYSCON_I2SDIV_MDIV_MASK) },
    {  76, SYSCON_I2SDIV_PDIV_2  | ( 19 & SYSCON_I2SDIV_MDIV_MASK) },
    {  78, SYSCON_I2SDIV_PDIV_3  | ( 13 & SYSCON_I2SDIV_MDIV_MASK) },
    {  80, SYSCON_I2SDIV_PDIV_25 | ( 16 & SYSCON_I2SDIV_MDIV_MASK) },
    {  84, SYSCON_I2SDIV_PDIV_3  | ( 14 & SYSCON_I2SDIV_MDIV_MASK) },
    {  85, SYSCON_I2SDIV_PDIV_25 | ( 17 & SYSCON_I2SDIV_MDIV_MASK) },
    {  88, SYSCON_I2SDIV_PDIV_2  | ( 22 & SYSCON_I2SDIV_MDIV_MASK) },
    {  90, SYSCON_I2SDIV_PDIV_3  | ( 15 & SYSCON_I2SDIV_MDIV_MASK) },
    {  92, SYSCON_I2SDIV_PDIV_2  | ( 23 & SYSCON_I2SDIV_MDIV_MASK) },
    {  95, SYSCON_I2SDIV_PDIV_25 | ( 19 & SYSCON_I2SDIV_MDIV_MASK) },
    {  96, SYSCON_I2SDIV_PDIV_3  | ( 16 & SYSCON_I2SDIV_MDIV_MASK) },
    { 100, SYSCON_I2SDIV_PDIV_25 | ( 20 & SYSCON_I2SDIV_MDIV_MASK) },
    { 102, SYSCON_I2SDIV_PDIV_3  | ( 17 & SYSCON_I2SDIV_MDIV_MASK) },
    { 104, SYSCON_I2SDIV_PDIV_2  | ( 26 & SYSCON_I2SDIV_MDIV_MASK) },
    { 105, SYSCON_I2SDIV_PDIV_25 | ( 21 & SYSCON_I2SDIV_MDIV_MASK) },
    { 108, SYSCON_I2SDIV_PDIV_3  | ( 18 & SYSCON_I2SDIV_MDIV_MASK) },
    { 110, SYSCON_I2SDIV_PDIV_25 | ( 22 & SYSCON_I2SDIV_MDIV_MASK) },
    { 112, SYSCON_I2SDIV_PDIV_2  | ( 28 & SYSCON_I2SDIV_MDIV_MASK) },
    { 114, SYSCON_I2SDIV_PDIV_3  | ( 19 & SYSCON_I2SDIV_MDIV_MASK) },
    { 115, SYSCON_I2SDIV_PDIV_25 | ( 23 & SYSCON_I2SDIV_MDIV_MASK) },
    { 116, SYSCON_I2SDIV_PDIV_2  | ( 29 & SYSCON_I2SDIV_MDIV_MASK) },
    { 120, SYSCON_I2SDIV_PDIV_3  | ( 20 & SYSCON_I2SDIV_MDIV_MASK) },
    { 124, SYSCON_I2SDIV_PDIV_2  | ( 31 & SYSCON_I2SDIV_MDIV_MASK) },
    { 125, SYSCON_I2SDIV_PDIV_25 | ( 25 & SYSCON_I2SDIV_MDIV_MASK) },
    { 126, SYSCON_I2SDIV_PDIV_3  | ( 21 & SYSCON_I2SDIV_MDIV_MASK) },
    { 128, SYSCON_I2SDIV_PDIV_2  | ( 32 & SYSCON_I2SDIV_MDIV_MASK) },
    { 130, SYSCON_I2SDIV_PDIV_25 | ( 26 & SYSCON_I2SDIV_MDIV_MASK) },
    { 132, SYSCON_I2SDIV_PDIV_3  | ( 22 & SYSCON_I2SDIV_MDIV_MASK) },
    { 135, SYSCON_I2SDIV_PDIV_25 | ( 27 & SYSCON_I2SDIV_MDIV_MASK) },
    { 136, SYSCON_I2SDIV_PDIV_2  | ( 34 & SYSCON_I2SDIV_MDIV_MASK) },
    { 138, SYSCON_I2SDIV_PDIV_3  | ( 23 & SYSCON_I2SDIV_MDIV_MASK) },
    { 140, SYSCON_I2SDIV_PDIV_25 | ( 28 & SYSCON_I2SDIV_MDIV_MASK) },
    { 144, SYSCON_I2SDIV_PDIV_3  | ( 24 & SYSCON_I2SDIV_MDIV_MASK) },
    { 145, SYSCON_I2SDIV_PDIV_25 | ( 29 & SYSCON_I2SDIV_MDIV_MASK) },
    { 148, SYSCON_I2SDIV_PDIV_2  | ( 37 & SYSCON_I2SDIV_MDIV_MASK) },
    { 150, SYSCON_I2SDIV_PDIV_3  | ( 25 & SYSCON_I2SDIV_MDIV_MASK) },
    { 152, SYSCON_I2SDIV_PDIV_2  | ( 38 & SYSCON_I2SDIV_MDIV_MASK) },
    { 155, SYSCON_I2SDIV_PDIV_25 | ( 31 & SYSCON_I2SDIV_MDIV_MASK) },
    { 156, SYSCON_I2SDIV_PDIV_3  | ( 26 & SYSCON_I2SDIV_MDIV_MASK) },
    { 160, SYSCON_I2SDIV_PDIV_25 | ( 32 & SYSCON_I2SDIV_MDIV_MASK) },
    { 162, SYSCON_I2SDIV_PDIV_3  | ( 27 & SYSCON_I2SDIV_MDIV_MASK) },
    { 164, SYSCON_I2SDIV_PDIV_2  | ( 41 & SYSCON_I2SDIV_MDIV_MASK) },
    { 165, SYSCON_I2SDIV_PDIV_25 | ( 33 & SYSCON_I2SDIV_MDIV_MASK) },
    { 168, SYSCON_I2SDIV_PDIV_3  | ( 28 & SYSCON_I2SDIV_MDIV_MASK) },
    { 170, SYSCON_I2SDIV_PDIV_25 | ( 34 & SYSCON_I2SDIV_MDIV_MASK) },
    { 172, SYSCON_I2SDIV_PDIV_2  | ( 43 & SYSCON_I2SDIV_MDIV_MASK) },
    { 174, SYSCON_I2SDIV_PDIV_3  | ( 29 & SYSCON_I2SDIV_MDIV_MASK) },
    { 175, SYSCON_I2SDIV_PDIV_25 | ( 35 & SYSCON_I2SDIV_MDIV_MASK) },
    { 176, SYSCON_I2SDIV_PDIV_2  | ( 44 & SYSCON_I2SDIV_MDIV_MASK) },
    { 180, SYSCON_I2SDIV_PDIV_3  | ( 30 & SYSCON_I2SDIV_MDIV_MASK) },
    { 184, SYSCON_I2SDIV_PDIV_2  | ( 46 & SYSCON_I2SDIV_MDIV_MASK) },
    { 185, SYSCON_I2SDIV_PDIV_25 | ( 37 & SYSCON_I2SDIV_MDIV_MASK) },
    { 186, SYSCON_I2SDIV_PDIV_3  | ( 31 & SYSCON_I2SDIV_MDIV_MASK) },
    { 188, SYSCON_I2SDIV_PDIV_2  | ( 47 & SYSCON_I2SDIV_MDIV_MASK) },
    { 190, SYSCON_I2SDIV_PDIV_25 | ( 38 & SYSCON_I2SDIV_MDIV_MASK) },
    { 192, SYSCON_I2SDIV_PDIV_3  | ( 32 & SYSCON_I2SDIV_MDIV_MASK) },
    { 195, SYSCON_I2SDIV_PDIV_25 | ( 39 & SYSCON_I2SDIV_MDIV_MASK) },
    { 196, SYSCON_I2SDIV_PDIV_2  | ( 49 & SYSCON_I2SDIV_MDIV_MASK) },
    { 198, SYSCON_I2SDIV_PDIV_3  | ( 33 & SYSCON_I2SDIV_MDIV_MASK) },
    { 200, SYSCON_I2SDIV_PDIV_25 | ( 40 & SYSCON_I2SDIV_MDIV_MASK) },
    { 204, SYSCON_I2SDIV_PDIV_3  | ( 34 & SYSCON_I2SDIV_MDIV_MASK) },
    { 205, SYSCON_I2SDIV_PDIV_25 | ( 41 & SYSCON_I2SDIV_MDIV_MASK) },
    { 208, SYSCON_I2SDIV_PDIV_2  | ( 52 & SYSCON_I2SDIV_MDIV_MASK) },
    { 210, SYSCON_I2SDIV_PDIV_3  | ( 35 & SYSCON_I2SDIV_MDIV_MASK) },
    { 212, SYSCON_I2SDIV_PDIV_2  | ( 53 & SYSCON_I2SDIV_MDIV_MASK) },
    { 215, SYSCON_I2SDIV_PDIV_25 | ( 43 & SYSCON_I2SDIV_MDIV_MASK) },
    { 216, SYSCON_I2SDIV_PDIV_3  | ( 36 & SYSCON_I2SDIV_MDIV_MASK) },
    { 220, SYSCON_I2SDIV_PDIV_25 | ( 44 & SYSCON_I2SDIV_MDIV_MASK) },
    { 222, SYSCON_I2SDIV_PDIV_3  | ( 37 & SYSCON_I2SDIV_MDIV_MASK) },
    { 224, SYSCON_I2SDIV_PDIV_2  | ( 56 & SYSCON_I2SDIV_MDIV_MASK) },
    { 225, SYSCON_I2SDIV_PDIV_25 | ( 45 & SYSCON_I2SDIV_MDIV_MASK) },
    { 228, SYSCON_I2SDIV_PDIV_3  | ( 38 & SYSCON_I2SDIV_MDIV_MASK) },
    { 230, SYSCON_I2SDIV_PDIV_25 | ( 46 & SYSCON_I2SDIV_MDIV_MASK) },
    { 232, SYSCON_I2SDIV_PDIV_2  | ( 58 & SYSCON_I2SDIV_MDIV_MASK) },
    { 234, SYSCON_I2SDIV_PDIV_3  | ( 39 & SYSCON_I2SDIV_MDIV_MASK) },
    { 235, SYSCON_I2SDIV_PDIV_25 | ( 47 & SYSCON_I2SDIV_MDIV_MASK) },
    { 236, SYSCON_I2SDIV_PDIV_2  | ( 59 & SYSCON_I2SDIV_MDIV_MASK) },
    { 240, SYSCON_I2SDIV_PDIV_3  | ( 40 & SYSCON_I2SDIV_MDIV_MASK) },
    { 244, SYSCON_I2SDIV_PDIV_2  | ( 61 & SYSCON_I2SDIV_MDIV_MASK) },
    { 245, SYSCON_I2SDIV_PDIV_25 | ( 49 & SYSCON_I2SDIV_MDIV_MASK) },
    { 246, SYSCON_I2SDIV_PDIV_3  | ( 41 & SYSCON_I2SDIV_MDIV_MASK) },
    { 248, SYSCON_I2SDIV_PDIV_2  | ( 62 & SYSCON_I2SDIV_MDIV_MASK) },
    { 250, SYSCON_I2SDIV_PDIV_25 | ( 50 & SYSCON_I2SDIV_MDIV_MASK) },
    { 252, SYSCON_I2SDIV_PDIV_3  | ( 42 & SYSCON_I2SDIV_MDIV_MASK) },
    { 255, SYSCON_I2SDIV_PDIV_25 | ( 51 & SYSCON_I2SDIV_MDIV_MASK) },
    { 256, SYSCON_I2SDIV_PDIV_2  | ( 64 & SYSCON_I2SDIV_MDIV_MASK) },
    { 258, SYSCON_I2SDIV_PDIV_3  | ( 43 & SYSCON_I2SDIV_MDIV_MASK) },
    { 260, SYSCON_I2SDIV_PDIV_25 | ( 52 & SYSCON_I2SDIV_MDIV_MASK) },
    { 264, SYSCON_I2SDIV_PDIV_3  | ( 44 & SYSCON_I2SDIV_MDIV_MASK) },
    { 265, SYSCON_I2SDIV_PDIV_25 | ( 53 & SYSCON_I2SDIV_MDIV_MASK) },
    { 268, SYSCON_I2SDIV_PDIV_2  | ( 67 & SYSCON_I2SDIV_MDIV_MASK) },
    { 270, SYSCON_I2SDIV_PDIV_3  | ( 45 & SYSCON_I2SDIV_MDIV_MASK) },
    { 272, SYSCON_I2SDIV_PDIV_2  | ( 68 & SYSCON_I2SDIV_MDIV_MASK) },
    { 275, SYSCON_I2SDIV_PDIV_25 | ( 55 & SYSCON_I2SDIV_MDIV_MASK) },
    { 276, SYSCON_I2SDIV_PDIV_3  | ( 46 & SYSCON_I2SDIV_MDIV_MASK) },
    { 280, SYSCON_I2SDIV_PDIV_25 | ( 56 & SYSCON_I2SDIV_MDIV_MASK) },
    { 282, SYSCON_I2SDIV_PDIV_3  | ( 47 & SYSCON_I2SDIV_MDIV_MASK) },
    { 284, SYSCON_I2SDIV_PDIV_2  | ( 71 & SYSCON_I2SDIV_MDIV_MASK) },
    { 285, SYSCON_I2SDIV_PDIV_25 | ( 57 & SYSCON_I2SDIV_MDIV_MASK) },
    { 288, SYSCON_I2SDIV_PDIV_3  | ( 48 & SYSCON_I2SDIV_MDIV_MASK) },
    { 290, SYSCON_I2SDIV_PDIV_25 | ( 58 & SYSCON_I2SDIV_MDIV_MASK) },
    { 292, SYSCON_I2SDIV_PDIV_2  | ( 73 & SYSCON_I2SDIV_MDIV_MASK) },
    { 294, SYSCON_I2SDIV_PDIV_3  | ( 49 & SYSCON_I2SDIV_MDIV_MASK) },
    { 295, SYSCON_I2SDIV_PDIV_25 | ( 59 & SYSCON_I2SDIV_MDIV_MASK) },
    { 296, SYSCON_I2SDIV_PDIV_2  | ( 74 & SYSCON_I2SDIV_MDIV_MASK) },
    { 300, SYSCON_I2SDIV_PDIV_3  | ( 50 & SYSCON_I2SDIV_MDIV_MASK) },
    { 304, SYSCON_I2SDIV_PDIV_2  | ( 76 & SYSCON_I2SDIV_MDIV_MASK) },
    { 305, SYSCON_I2SDIV_PDIV_25 | ( 61 & SYSCON_I2SDIV_MDIV_MASK) },
    { 306, SYSCON_I2SDIV_PDIV_3  | ( 51 & SYSCON_I2SDIV_MDIV_MASK) },
    { 308, SYSCON_I2SDIV_PDIV_2  | ( 77 & SYSCON_I2SDIV_MDIV_MASK) },
    { 310, SYSCON_I2SDIV_PDIV_25 | ( 62 & SYSCON_I2SDIV_MDIV_MASK) },
    { 312, SYSCON_I2SDIV_PDIV_3  | ( 52 & SYSCON_I2SDIV_MDIV_MASK) },
    { 315, SYSCON_I2SDIV_PDIV_25 | ( 63 & SYSCON_I2SDIV_MDIV_MASK) },
    { 316, SYSCON_I2SDIV_PDIV_2  | ( 79 & SYSCON_I2SDIV_MDIV_MASK) },
    { 318, SYSCON_I2SDIV_PDIV_3  | ( 53 & SYSCON_I2SDIV_MDIV_MASK) },
    { 320, SYSCON_I2SDIV_PDIV_25 | ( 64 & SYSCON_I2SDIV_MDIV_MASK) },
    { 324, SYSCON_I2SDIV_PDIV_3  | ( 54 & SYSCON_I2SDIV_MDIV_MASK) },
    { 325, SYSCON_I2SDIV_PDIV_25 | ( 65 & SYSCON_I2SDIV_MDIV_MASK) },
    { 328, SYSCON_I2SDIV_PDIV_2  | ( 82 & SYSCON_I2SDIV_MDIV_MASK) },
    { 330, SYSCON_I2SDIV_PDIV_3  | ( 55 & SYSCON_I2SDIV_MDIV_MASK) },
    { 332, SYSCON_I2SDIV_PDIV_2  | ( 83 & SYSCON_I2SDIV_MDIV_MASK) },
    { 335, SYSCON_I2SDIV_PDIV_25 | ( 67 & SYSCON_I2SDIV_MDIV_MASK) },
    { 336, SYSCON_I2SDIV_PDIV_3  | ( 56 & SYSCON_I2SDIV_MDIV_MASK) },
    { 340, SYSCON_I2SDIV_PDIV_25 | ( 68 & SYSCON_I2SDIV_MDIV_MASK) },
    { 342, SYSCON_I2SDIV_PDIV_3  | ( 57 & SYSCON_I2SDIV_MDIV_MASK) },
    { 344, SYSCON_I2SDIV_PDIV_2  | ( 86 & SYSCON_I2SDIV_MDIV_MASK) },
    { 345, SYSCON_I2SDIV_PDIV_25 | ( 69 & SYSCON_I2SDIV_MDIV_MASK) },
    { 348, SYSCON_I2SDIV_PDIV_3  | ( 58 & SYSCON_I2SDIV_MDIV_MASK) },
    { 350, SYSCON_I2SDIV_PDIV_25 | ( 70 & SYSCON_I2SDIV_MDIV_MASK) },
    { 352, SYSCON_I2SDIV_PDIV_2  | ( 88 & SYSCON_I2SDIV_MDIV_MASK) },
    { 354, SYSCON_I2SDIV_PDIV_3  | ( 59 & SYSCON_I2SDIV_MDIV_MASK) },
    { 355, SYSCON_I2SDIV_PDIV_25 | ( 71 & SYSCON_I2SDIV_MDIV_MASK) },
    { 356, SYSCON_I2SDIV_PDIV_2  | ( 89 & SYSCON_I2SDIV_MDIV_MASK) },
    { 360, SYSCON_I2SDIV_PDIV_3  | ( 60 & SYSCON_I2SDIV_MDIV_MASK) },
    { 364, SYSCON_I2SDIV_PDIV_2  | ( 91 & SYSCON_I2SDIV_MDIV_MASK) },
    { 365, SYSCON_I2SDIV_PDIV_25 | ( 73 & SYSCON_I2SDIV_MDIV_MASK) },
    { 366, SYSCON_I2SDIV_PDIV_3  | ( 61 & SYSCON_I2SDIV_MDIV_MASK) },
    { 368, SYSCON_I2SDIV_PDIV_2  | ( 92 & SYSCON_I2SDIV_MDIV_MASK) },
    { 370, SYSCON_I2SDIV_PDIV_25 | ( 74 & SYSCON_I2SDIV_MDIV_MASK) },
    { 372, SYSCON_I2SDIV_PDIV_3  | ( 62 & SYSCON_I2SDIV_MDIV_MASK) },
    { 375, SYSCON_I2SDIV_PDIV_25 | ( 75 & SYSCON_I2SDIV_MDIV_MASK) },
    { 376, SYSCON_I2SDIV_PDIV_2  | ( 94 & SYSCON_I2SDIV_MDIV_MASK) },
    { 378, SYSCON_I2SDIV_PDIV_3  | ( 63 & SYSCON_I2SDIV_MDIV_MASK) },
    { 380, SYSCON_I2SDIV_PDIV_25 | ( 76 & SYSCON_I2SDIV_MDIV_MASK) },
    { 384, SYSCON_I2SDIV_PDIV_3  | ( 64 & SYSCON_I2SDIV_MDIV_MASK) },
    { 385, SYSCON_I2SDIV_PDIV_25 | ( 77 & SYSCON_I2SDIV_MDIV_MASK) },
    { 388, SYSCON_I2SDIV_PDIV_2  | ( 97 & SYSCON_I2SDIV_MDIV_MASK) },
    { 390, SYSCON_I2SDIV_PDIV_3  | ( 65 & SYSCON_I2SDIV_MDIV_MASK) },
    { 392, SYSCON_I2SDIV_PDIV_2  | ( 98 & SYSCON_I2SDIV_MDIV_MASK) },
    { 395, SYSCON_I2SDIV_PDIV_25 | ( 79 & SYSCON_I2SDIV_MDIV_MASK) },
    { 396, SYSCON_I2SDIV_PDIV_3  | ( 66 & SYSCON_I2SDIV_MDIV_MASK) },
    { 400, SYSCON_I2SDIV_PDIV_25 | ( 80 & SYSCON_I2SDIV_MDIV_MASK) },
    { 402, SYSCON_I2SDIV_PDIV_3  | ( 67 & SYSCON_I2SDIV_MDIV_MASK) },
    { 404, SYSCON_I2SDIV_PDIV_2  | (101 & SYSCON_I2SDIV_MDIV_MASK) },
    { 405, SYSCON_I2SDIV_PDIV_25 | ( 81 & SYSCON_I2SDIV_MDIV_MASK) },
    { 408, SYSCON_I2SDIV_PDIV_3  | ( 68 & SYSCON_I2SDIV_MDIV_MASK) },
    { 410, SYSCON_I2SDIV_PDIV_25 | ( 82 & SYSCON_I2SDIV_MDIV_MASK) },
    { 412, SYSCON_I2SDIV_PDIV_2  | (103 & SYSCON_I2SDIV_MDIV_MASK) },
    { 414, SYSCON_I2SDIV_PDIV_3  | ( 69 & SYSCON_I2SDIV_MDIV_MASK) },
    { 415, SYSCON_I2SDIV_PDIV_25 | ( 83 & SYSCON_I2SDIV_MDIV_MASK) },
    { 416, SYSCON_I2SDIV_PDIV_2  | (104 & SYSCON_I2SDIV_MDIV_MASK) },
    { 420, SYSCON_I2SDIV_PDIV_3  | ( 70 & SYSCON_I2SDIV_MDIV_MASK) },
    { 424, SYSCON_I2SDIV_PDIV_2  | (106 & SYSCON_I2SDIV_MDIV_MASK) },
    { 425, SYSCON_I2SDIV_PDIV_25 | ( 85 & SYSCON_I2SDIV_MDIV_MASK) },
    { 426, SYSCON_I2SDIV_PDIV_3  | ( 71 & SYSCON_I2SDIV_MDIV_MASK) },
    { 428, SYSCON_I2SDIV_PDIV_2  | (107 & SYSCON_I2SDIV_MDIV_MASK) },
    { 430, SYSCON_I2SDIV_PDIV_25 | ( 86 & SYSCON_I2SDIV_MDIV_MASK) },
    { 432, SYSCON_I2SDIV_PDIV_3  | ( 72 & SYSCON_I2SDIV_MDIV_MASK) },
    { 435, SYSCON_I2SDIV_PDIV_25 | ( 87 & SYSCON_I2SDIV_MDIV_MASK) },
    { 436, SYSCON_I2SDIV_PDIV_2  | (109 & SYSCON_I2SDIV_MDIV_MASK) },
    { 438, SYSCON_I2SDIV_PDIV_3  | ( 73 & SYSCON_I2SDIV_MDIV_MASK) },
    { 440, SYSCON_I2SDIV_PDIV_25 | ( 88 & SYSCON_I2SDIV_MDIV_MASK) },
    { 444, SYSCON_I2SDIV_PDIV_3  | ( 74 & SYSCON_I2SDIV_MDIV_MASK) },
    { 445, SYSCON_I2SDIV_PDIV_25 | ( 89 & SYSCON_I2SDIV_MDIV_MASK) },
    { 448, SYSCON_I2SDIV_PDIV_2  | (112 & SYSCON_I2SDIV_MDIV_MASK) },
    { 450, SYSCON_I2SDIV_PDIV_3  | ( 75 & SYSCON_I2SDIV_MDIV_MASK) },
    { 452, SYSCON_I2SDIV_PDIV_2  | (113 & SYSCON_I2SDIV_MDIV_MASK) },
    { 455, SYSCON_I2SDIV_PDIV_25 | ( 91 & SYSCON_I2SDIV_MDIV_MASK) },
    { 456, SYSCON_I2SDIV_PDIV_3  | ( 76 & SYSCON_I2SDIV_MDIV_MASK) },
    { 460, SYSCON_I2SDIV_PDIV_25 | ( 92 & SYSCON_I2SDIV_MDIV_MASK) },
    { 462, SYSCON_I2SDIV_PDIV_3  | ( 77 & SYSCON_I2SDIV_MDIV_MASK) },
    { 464, SYSCON_I2SDIV_PDIV_2  | (116 & SYSCON_I2SDIV_MDIV_MASK) },
    { 465, SYSCON_I2SDIV_PDIV_25 | ( 93 & SYSCON_I2SDIV_MDIV_MASK) },
    { 468, SYSCON_I2SDIV_PDIV_3  | ( 78 & SYSCON_I2SDIV_MDIV_MASK) },
    { 470, SYSCON_I2SDIV_PDIV_25 | ( 94 & SYSCON_I2SDIV_MDIV_MASK) },
    { 472, SYSCON_I2SDIV_PDIV_2  | (118 & SYSCON_I2SDIV_MDIV_MASK) },
    { 474, SYSCON_I2SDIV_PDIV_3  | ( 79 & SYSCON_I2SDIV_MDIV_MASK) },
    { 475, SYSCON_I2SDIV_PDIV_25 | ( 95 & SYSCON_I2SDIV_MDIV_MASK) },
    { 476, SYSCON_I2SDIV_PDIV_2  | (119 & SYSCON_I2SDIV_MDIV_MASK) },
    { 480, SYSCON_I2SDIV_PDIV_3  | ( 80 & SYSCON_I2SDIV_MDIV_MASK) },
    { 484, SYSCON_I2SDIV_PDIV_2  | (121 & SYSCON_I2SDIV_MDIV_MASK) },
    { 485, SYSCON_I2SDIV_PDIV_25 | ( 97 & SYSCON_I2SDIV_MDIV_MASK) },
    { 486, SYSCON_I2SDIV_PDIV_3  | ( 81 & SYSCON_I2SDIV_MDIV_MASK) },
    { 488, SYSCON_I2SDIV_PDIV_2  | (122 & SYSCON_I2SDIV_MDIV_MASK) },
    { 490, SYSCON_I2SDIV_PDIV_25 | ( 98 & SYSCON_I2SDIV_MDIV_MASK) },
    { 492, SYSCON_I2SDIV_PDIV_3  | ( 82 & SYSCON_I2SDIV_MDIV_MASK) },
    { 495, SYSCON_I2SDIV_PDIV_25 | ( 99 & SYSCON_I2SDIV_MDIV_MASK) },
    { 496, SYSCON_I2SDIV_PDIV_2  | (124 & SYSCON_I2SDIV_MDIV_MASK) },
    { 498, SYSCON_I2SDIV_PDIV_3  | ( 83 & SYSCON_I2SDIV_MDIV_MASK) },
    { 500, SYSCON_I2SDIV_PDIV_25 | (100 & SYSCON_I2SDIV_MDIV_MASK) },
    { 504, SYSCON_I2SDIV_PDIV_3  | ( 84 & SYSCON_I2SDIV_MDIV_MASK) },
    { 505, SYSCON_I2SDIV_PDIV_25 | (101 & SYSCON_I2SDIV_MDIV_MASK) },
    { 508, SYSCON_I2SDIV_PDIV_2  | (127 & SYSCON_I2SDIV_MDIV_MASK) },
    { 510, SYSCON_I2SDIV_PDIV_3  | ( 85 & SYSCON_I2SDIV_MDIV_MASK) },
    { 515, SYSCON_I2SDIV_PDIV_25 | (103 & SYSCON_I2SDIV_MDIV_MASK) },
    { 516, SYSCON_I2SDIV_PDIV_3  | ( 86 & SYSCON_I2SDIV_MDIV_MASK) },
    { 520, SYSCON_I2SDIV_PDIV_25 | (104 & SYSCON_I2SDIV_MDIV_MASK) },
    { 522, SYSCON_I2SDIV_PDIV_3  | ( 87 & SYSCON_I2SDIV_MDIV_MASK) },
    { 525, SYSCON_I2SDIV_PDIV_25 | (105 & SYSCON_I2SDIV_MDIV_MASK) },
    { 528, SYSCON_I2SDIV_PDIV_3  | ( 88 & SYSCON_I2SDIV_MDIV_MASK) },
    { 530, SYSCON_I2SDIV_PDIV_25 | (106 & SYSCON_I2SDIV_MDIV_MASK) },
    { 534, SYSCON_I2SDIV_PDIV_3  | ( 89 & SYSCON_I2SDIV_MDIV_MASK) },
    { 535, SYSCON_I2SDIV_PDIV_25 | (107 & SYSCON_I2SDIV_MDIV_MASK) },
    { 540, SYSCON_I2SDIV_PDIV_3  | ( 90 & SYSCON_I2SDIV_MDIV_MASK) },
    { 545, SYSCON_I2SDIV_PDIV_25 | (109 & SYSCON_I2SDIV_MDIV_MASK) },
    { 546, SYSCON_I2SDIV_PDIV_3  | ( 91 & SYSCON_I2SDIV_MDIV_MASK) },
    { 550, SYSCON_I2SDIV_PDIV_25 | (110 & SYSCON_I2SDIV_MDIV_MASK) },
    { 552, SYSCON_I2SDIV_PDIV_3  | ( 92 & SYSCON_I2SDIV_MDIV_MASK) },
    { 555, SYSCON_I2SDIV_PDIV_25 | (111 & SYSCON_I2SDIV_MDIV_MASK) },
    { 558, SYSCON_I2SDIV_PDIV_3  | ( 93 & SYSCON_I2SDIV_MDIV_MASK) },
    { 560, SYSCON_I2SDIV_PDIV_25 | (112 & SYSCON_I2SDIV_MDIV_MASK) },
    { 564, SYSCON_I2SDIV_PDIV_3  | ( 94 & SYSCON_I2SDIV_MDIV_MASK) },
    { 565, SYSCON_I2SDIV_PDIV_25 | (113 & SYSCON_I2SDIV_MDIV_MASK) },
    { 570, SYSCON_I2SDIV_PDIV_3  | ( 95 & SYSCON_I2SDIV_MDIV_MASK) },
    { 575, SYSCON_I2SDIV_PDIV_25 | (115 & SYSCON_I2SDIV_MDIV_MASK) },
    { 576, SYSCON_I2SDIV_PDIV_3  | ( 96 & SYSCON_I2SDIV_MDIV_MASK) },
    { 580, SYSCON_I2SDIV_PDIV_25 | (116 & SYSCON_I2SDIV_MDIV_MASK) },
    { 582, SYSCON_I2SDIV_PDIV_3  | ( 97 & SYSCON_I2SDIV_MDIV_MASK) },
    { 585, SYSCON_I2SDIV_PDIV_25 | (117 & SYSCON_I2SDIV_MDIV_MASK) },
    { 588, SYSCON_I2SDIV_PDIV_3  | ( 98 & SYSCON_I2SDIV_MDIV_MASK) },
    { 590, SYSCON_I2SDIV_PDIV_25 | (118 & SYSCON_I2SDIV_MDIV_MASK) },
    { 594, SYSCON_I2SDIV_PDIV_3  | ( 99 & SYSCON_I2SDIV_MDIV_MASK) },
    { 595, SYSCON_I2SDIV_PDIV_25 | (119 & SYSCON_I2SDIV_MDIV_MASK) },
    { 600, SYSCON_I2SDIV_PDIV_3  | (100 & SYSCON_I2SDIV_MDIV_MASK) },
    { 605, SYSCON_I2SDIV_PDIV_25 | (121 & SYSCON_I2SDIV_MDIV_MASK) },
    { 606, SYSCON_I2SDIV_PDIV_3  | (101 & SYSCON_I2SDIV_MDIV_MASK) },
    { 610, SYSCON_I2SDIV_PDIV_25 | (122 & SYSCON_I2SDIV_MDIV_MASK) },
    { 612, SYSCON_I2SDIV_PDIV_3  | (102 & SYSCON_I2SDIV_MDIV_MASK) },
    { 615, SYSCON_I2SDIV_PDIV_25 | (123 & SYSCON_I2SDIV_MDIV_MASK) },
    { 618, SYSCON_I2SDIV_PDIV_3  | (103 & SYSCON_I2SDIV_MDIV_MASK) },
    { 620, SYSCON_I2SDIV_PDIV_25 | (124 & SYSCON_I2SDIV_MDIV_MASK) },
    { 624, SYSCON_I2SDIV_PDIV_3  | (104 & SYSCON_I2SDIV_MDIV_MASK) },
    { 625, SYSCON_I2SDIV_PDIV_25 | (125 & SYSCON_I2SDIV_MDIV_MASK) },
    { 630, SYSCON_I2SDIV_PDIV_3  | (105 & SYSCON_I2SDIV_MDIV_MASK) },
    { 635, SYSCON_I2SDIV_PDIV_25 | (127 & SYSCON_I2SDIV_MDIV_MASK) },
    { 636, SYSCON_I2SDIV_PDIV_3  | (106 & SYSCON_I2SDIV_MDIV_MASK) },
    { 642, SYSCON_I2SDIV_PDIV_3  | (107 & SYSCON_I2SDIV_MDIV_MASK) },
    { 648, SYSCON_I2SDIV_PDIV_3  | (108 & SYSCON_I2SDIV_MDIV_MASK) },
    { 654, SYSCON_I2SDIV_PDIV_3  | (109 & SYSCON_I2SDIV_MDIV_MASK) },
    { 660, SYSCON_I2SDIV_PDIV_3  | (110 & SYSCON_I2SDIV_MDIV_MASK) },
    { 666, SYSCON_I2SDIV_PDIV_3  | (111 & SYSCON_I2SDIV_MDIV_MASK) },
    { 672, SYSCON_I2SDIV_PDIV_3  | (112 & SYSCON_I2SDIV_MDIV_MASK) },
    { 678, SYSCON_I2SDIV_PDIV_3  | (113 & SYSCON_I2SDIV_MDIV_MASK) },
    { 684, SYSCON_I2SDIV_PDIV_3  | (114 & SYSCON_I2SDIV_MDIV_MASK) },
    { 690, SYSCON_I2SDIV_PDIV_3  | (115 & SYSCON_I2SDIV_MDIV_MASK) },
    { 696, SYSCON_I2SDIV_PDIV_3  | (116 & SYSCON_I2SDIV_MDIV_MASK) },
    { 702, SYSCON_I2SDIV_PDIV_3  | (117 & SYSCON_I2SDIV_MDIV_MASK) },
    { 708, SYSCON_I2SDIV_PDIV_3  | (118 & SYSCON_I2SDIV_MDIV_MASK) },
    { 714, SYSCON_I2SDIV_PDIV_3  | (119 & SYSCON_I2SDIV_MDIV_MASK) },
    { 720, SYSCON_I2SDIV_PDIV_3  | (120 & SYSCON_I2SDIV_MDIV_MASK) },
    { 726, SYSCON_I2SDIV_PDIV_3  | (121 & SYSCON_I2SDIV_MDIV_MASK) },
    { 732, SYSCON_I2SDIV_PDIV_3  | (122 & SYSCON_I2SDIV_MDIV_MASK) },
    { 738, SYSCON_I2SDIV_PDIV_3  | (123 & SYSCON_I2SDIV_MDIV_MASK) },
    { 744, SYSCON_I2SDIV_PDIV_3  | (124 & SYSCON_I2SDIV_MDIV_MASK) },
    { 750, SYSCON_I2SDIV_PDIV_3  | (125 & SYSCON_I2SDIV_MDIV_MASK) },
    { 756, SYSCON_I2SDIV_PDIV_3  | (126 & SYSCON_I2SDIV_MDIV_MASK) },
    { 762, SYSCON_I2SDIV_PDIV_3  | (127 & SYSCON_I2SDIV_MDIV_MASK) }
};

/*
 * When we get to the multichannel case the pre-fill and enable code
 * will go to the dma driver's start routine.
 */
static void ep93xx_audio_enable_all(void)
{
    int i;
	unsigned long ulTemp;
	
	DPRINTK("ep93xx_audio_enable_all - enter\n");

	/*
	 * Disable the channels, disable the i2s controller to clear out the 
	 * fifo.  Disabling the controller will clear the flags	in i2s_csr.
	 */
	outl( 0, I2STX0En );
	outl( 0, I2STX1En );
	outl( 0, I2STX2En );
	outl( 0, I2SRX0En );
	outl( 0, I2SRX1En );
	outl( 0, I2SRX2En );
	ulTemp = inl( I2SRX2En ); /* input to push the outl's past the wrapper */
	
	/*
	 * Disable the i2s controller to clear out the fifo and clear
	 * the flags in i2s_csr.
	 */
	outl( 0, I2SGlCtrl );
	ulTemp = inl( I2SGlCtrl );

	/*
	 * Enable the i2s controller.  It is important not to just
	 * leave the i2s controller enabled and skip this step. 
	 */
	outl( 1, I2SGlCtrl );
	ulTemp = inl( I2SGlCtrl );

	/*
	 * Prefill the tx fifo with zeros.
	 */
    for( i = 0 ; i < 8 ; i++ )
    {
		outl( 0, I2STX0Lft );
		outl( 0, I2STX0Rt );
		outl( 0, I2STX1Lft );
		outl( 0, I2STX1Rt );
		outl( 0, I2STX2Lft );
		outl( 0, I2STX2Rt );
		ulTemp = inl( I2SRX2En ); /* input to push the outl's past the wrapper */
    }

    /*
	 * Enable the tx channels.
	 */
	outl( 1, I2STX0En );
	outl( 1, I2STX1En );
	outl( 1, I2STX2En );
	outl( 1, I2SRX0En );
	outl( 1, I2SRX1En );
	outl( 1, I2SRX2En );
	ulTemp = inl( I2SRX2En ); /* input to push the outl's past the wrapper */

	DPRINTK("ep93xx_audio_enable_all - EXIT\n");

}

static void ep93xx_audio_disable_all(void)
{
	unsigned long ulTemp;
	
	DPRINTK("ep93xx_audio_disable_all - enter\n");

	/*
	 * Disable the channels, disable the i2s controller to clear out the 
	 * fifo.  Disabling the controller will clear the flags	in i2s_csr.
	 */
	outl( 0, I2STX0En );
	outl( 0, I2STX1En );
	outl( 0, I2STX2En );
	outl( 0, I2SRX0En );
	outl( 0, I2SRX1En );
	outl( 0, I2SRX2En );
	outl( 0, I2SGlCtrl );
	ulTemp = inl( I2SRX2En ); /* input to push the outl's past the wrapper */

	DPRINTK("ep93xx_audio_disable_all - EXIT\n");
}

/*
 * ep93xx_clear_i2s_fifo
 *
 * This routine will clear out the transmit fifo of an audio playback stream 
 * (can be multi-channel) and re-enable it.
 */
static void ep93xx_clear_i2s_fifo( audio_stream_t * stream )
{
    int i;
	unsigned long ulTemp;
	
	DPRINTK("ep93xx_clear_i2s_fifo - enter\n");
	
	/*
	 * If this is a rx stream, don't need to do this.
	 */
	if( (stream->TX_dma_ch_bitfield & 0x7) == 0 )
		return;
	
	/*
	 * Disable the channel.
	 */
	if( stream->TX_dma_ch_bitfield & 0x1 )
		outl( 0, I2STX0En );

	if( stream->TX_dma_ch_bitfield & 0x2 )
		outl( 0, I2STX1En );

	if( stream->TX_dma_ch_bitfield & 0x4 )
		outl( 0, I2STX2En );

	ulTemp = inl( I2SRX2En ); /* input to push the outl's past the wrapper */

	/*
	 * Prefill the tx fifo with zeros.
	 */
    for( i = 0 ; i < 8 ; i++ )
    {
		if( stream->TX_dma_ch_bitfield & 0x1 )
		{
			outl( 0, I2STX0Lft );
			outl( 0, I2STX0Rt );
		}

		if( stream->TX_dma_ch_bitfield & 0x2 )
		{
			outl( 0, I2STX1Lft );
			outl( 0, I2STX1Rt );
		}

		if( stream->TX_dma_ch_bitfield & 0x4 )
		{
			outl( 0, I2STX2Lft );
			outl( 0, I2STX2Rt );
		}
		
		ulTemp = inl( I2SRX2En ); /* input to push the outl's past the wrapper */
    }

    /*
	 * Enable the tx channels.
	 */
	if( stream->TX_dma_ch_bitfield & 0x1 )
		outl( 1, I2STX0En );

	if( stream->TX_dma_ch_bitfield & 0x2 )
		outl( 1, I2STX1En );

	if( stream->TX_dma_ch_bitfield & 0x4 )
		outl( 1, I2STX2En );

	ulTemp = inl( I2SRX2En ); /* input to push the outl's past the wrapper */

	DPRINTK("ep93xx_clear_i2s_fifo - EXIT\n");
}

/*
 * ep93xx_calc_closest_freq
 * 
 *   ulPLLFreq           -  PLL output Frequency (Mhz)
 *   ulRequestedMClkFreq -  Requested Video Clock Frequency.
 *   pulActualMClkFreq   -  Returned Actual Video Rate.
 *   pulI2SDiv           -  Video Divider register.
 *
 *   Return            0 - Failure
 *                     1 - Success
 */
static int ep93xx_calc_closest_freq
(
    ulong   ulPLLFreq, 
    ulong   ulRequestedMClkFreq,
    ulong * pulActualMClkFreq,
    ulong * pulI2SDiv
)
{
    
    ulong   ulLower;
    ulong   ulUpper;
    ulong   ulDiv;
    int     x;

    /*
     * Calculate the closest divisor.
     */
    ulDiv =  (ulPLLFreq * 2)/ ulRequestedMClkFreq;

    for(x = 1; x < sizeof(I2SDivTable)/sizeof(DIV_TABLE); x++)
    {
        /*
         * Calculate the next greater and lower value.
         */
        ulLower = I2SDivTable[x - 1].ulTotalDiv;     
        ulUpper = I2SDivTable[x].ulTotalDiv;     

        /*
         * Check to see if it is in between the two values.
         */
        if(ulLower <= ulDiv && ulDiv < ulUpper)
        {
            break;
        }
    }

    /*
     * Return if we did not find a divisor.
     */
    if(x == sizeof(I2SDivTable)/sizeof(DIV_TABLE))
    {
		DPRINTK("couldn't find a divisor.\n");

        *pulActualMClkFreq  = 0;
        *pulI2SDiv          = 0;
        return -1;
    }

    /*
     * See which is closer, the upper or the lower case.
     */
    if(ulUpper * ulRequestedMClkFreq - ulPLLFreq * 2 >  
      ulPLLFreq * 2 - ulLower * ulRequestedMClkFreq)
    {
        x-=1;
    }
    *pulActualMClkFreq  = (ulPLLFreq * 2)/ I2SDivTable[x].ulTotalDiv;
    *pulI2SDiv          = I2SDivTable[x].ulI2SDiv;

    return 0;
}

/*
 * ep93xx_get_PLL_frequency
 *
 * Given a value for ClkSet1 or ClkSet2, calculate the PLL1 or PLL2 frequency.
 */
static ulong ep93xx_get_PLL_frequency( ulong ulCLKSET )
{
	ulong ulX1FBD, ulX2FBD, ulX2IPD, ulPS, ulPLL_Freq;
	
	ulPS = (ulCLKSET & SYSCON_CLKSET1_PLL1_PS_MASK) >> SYSCON_CLKSET1_PLL1_PS_SHIFT; 	
	ulX1FBD = (ulCLKSET & SYSCON_CLKSET1_PLL1_X1FBD1_MASK) >> SYSCON_CLKSET1_PLL1_X1FBD1_SHIFT;
	ulX2FBD = (ulCLKSET & SYSCON_CLKSET1_PLL1_X2FBD2_MASK) >> SYSCON_CLKSET1_PLL1_X2FBD2_SHIFT;
	ulX2IPD = (ulCLKSET & SYSCON_CLKSET1_PLL1_X2IPD_MASK) >> SYSCON_CLKSET1_PLL1_X2IPD_SHIFT;

	/*
	 * This may be off by a very small fraction of a percent.
	 */
	ulPLL_Freq = (((0x00e10000 * (ulX1FBD+1)) / (ulX2IPD+1)) * (ulX2FBD+1)) >> ulPS;

	return ulPLL_Freq;
}

/*
 * SetSampleRate
 * 		disables i2s channels and sets up i2s divisors
 * 		in syscon for the requested sample rate.
 *
 * lFrequency - Sample Rate in Hz
 */
static void ep93xx_set_samplerate(long lFrequency)
{
    ulong ulRequestedMClkFreq, ulPLL1_CLOCK, ulPLL2_CLOCK;
    ulong ulMClkFreq1, ulMClkFreq2, ulClkSet1, ulClkSet2;
    ulong ulI2SDiv, ulI2SDiv1, ulI2SDiv2, ulI2SDIV, actual_samplerate;
    
	/*
	 * Clock ratios: MCLK to SCLK and SCLK to LRCK
	 */
	ulong ulM2SClock  = 4;
    ulong ulS2LRClock = 64;

	DPRINTK( "ep93xx_set_samplerate = %d Hz.\n", (int)lFrequency );

	/*
	 * Read CLKSET1 and CLKSET2 in the System Controller and calculate
	 * the PLL frequencies from that.
	 */
	ulClkSet1 =	inl(SYSCON_CLKSET1);
	ulClkSet2 =	inl(SYSCON_CLKSET2);
	ulPLL1_CLOCK = ep93xx_get_PLL_frequency( ulClkSet1 );
	ulPLL2_CLOCK = ep93xx_get_PLL_frequency( ulClkSet2 );

	DPRINTK( "ClkSet1=0x%08x Clkset2=0x%08x  PLL1=%d Hz  PLL2=%d Hz\n", 
		(int)ulClkSet1, (int)ulClkSet2, (int)ulPLL1_CLOCK, (int)ulPLL2_CLOCK );

    ulRequestedMClkFreq = ( lFrequency * ulM2SClock * ulS2LRClock);

    ep93xx_calc_closest_freq
    (
        ulPLL1_CLOCK, 
        ulRequestedMClkFreq,
        &ulMClkFreq1,
        &ulI2SDiv1
    );
    ep93xx_calc_closest_freq
    (
        ulPLL2_CLOCK, 
        ulRequestedMClkFreq,
        &ulMClkFreq2,
        &ulI2SDiv2
    );

    /*
     * See which is closer, MClk rate 1 or MClk rate 2.
     */
    if(abs(ulMClkFreq1 - ulRequestedMClkFreq) < abs(ulMClkFreq2 -ulRequestedMClkFreq))
    {
        ulI2SDiv    = ulI2SDiv1;
        actual_samplerate = ulMClkFreq1/ (ulM2SClock * ulS2LRClock);
		DPRINTK( "ep93xx_set_samplerate - using PLL1\n" );
    }
    else
    {
        ulI2SDiv = ulI2SDiv2 | SYSCON_I2SDIV_PSEL;
        actual_samplerate = ulMClkFreq1 / (ulM2SClock * ulS2LRClock);
		DPRINTK( "ep93xx_set_samplerate - using PLL2\n" );
    }

    /*
     * Calculate the new I2SDIV register value and write it out.
     */
    ulI2SDIV = ulI2SDiv | SYSCON_I2SDIV_SENA |  SYSCON_I2SDIV_ORIDE | 
				SYSCON_I2SDIV_SPOL| SYSCON_I2SDIV_LRDIV_64 | 
				SYSCON_I2SDIV_SDIV | SYSCON_I2SDIV_MENA | SYSCON_I2SDIV_ESEL;

    DPRINTK("I2SDIV set to 0x%08x\n", (unsigned int)ulI2SDIV );
	
	/*
	 * For I2S all channels have same sample rate so we'll save them
	 * all in i2s_output_stream0.sample_rate.
	 */
	i2s_output_stream0.sample_rate = lFrequency;

    DPRINTK( "ep93xx_set_samplerate - requested %d Hz, got %d Hz, reporting %d Hz.\n",
	        (int)lFrequency, (int)actual_samplerate, (int)i2s_output_stream0.sample_rate );
			
	SysconSetLocked(SYSCON_I2SDIV, ulI2SDIV);
	audio_clocks_enabled_in_syscon = 1;

}

/*
 * BringUpI2S
 *
 * This routine sets up the I2S Controller.
 */
static void ep93xx_init_i2s_controller( void )
{
	unsigned int uiDEVCFG;
	
	DPRINTK("ep93xx_init_i2s_controller - enter\n");
	
    /*
     * Configure the multiplexed Ac'97 pins to be I2S.  Also configure 
     * EGPIO[4,5,6,13] to be SDIN's and SDOUT's for the second and third
     * I2S stereo channels if the codec is a 6 channel codec.
     */
	uiDEVCFG = inl(SYSCON_DEVCFG);
	
#ifdef CONFIG_CODEC_CS4271
	uiDEVCFG |= SYSCON_DEVCFG_I2SonAC97;
#else // CONFIG_CODEC_CS4228A
	uiDEVCFG |= SYSCON_DEVCFG_I2SonAC97 | 
		        SYSCON_DEVCFG_A1onG |
		        SYSCON_DEVCFG_A2onG;
#endif
	    
    SysconSetLocked(SYSCON_DEVCFG, uiDEVCFG);

    /* ------------- Configure I2S Tx channel ---------------------*/

    /*
	 * Justify Left, MSB first	
     */
	outl( 0, I2STXLinCtrlData );

	/*
	 * WL = 24 bits 
	 */
	outl( 1, I2STXWrdLen );

	/*
     * Set the I2S control block to master mode.
	 * Tx bit clk rate determined by word legnth 
	 * Do gate off last 8 clks (24 bit samples in a 32 bit field)
	 * LRclk = I2S timing; LRck = 0 for left
	 */
	outl( (i2s_txcc_mstr | i2s_txcc_trel), I2STxClkCfg );
	
    /* ------------- Configure I2S rx channel --------------------- */
    
    /*
     * First, clear all config bits.
     */
	outl( 0, I2SRXLinCtrlData );

	/*
	 * WL = 24 bits 
	 */
	outl( 1, I2SRXWrdLen );

	/* 
     * Set the I2S control block to master mode.
	 * Rx bit clk rate determined by word legnth 
	 * Do gate off last 8 clks 
	 * setting i2s_rxcc_rrel gives us I2S timing
	 * clearing i2s_rlrs gives us LRck = 0 for left, 1 for right
	 * setting i2s_rxcc_nbcg specifies to not gate off extra 8 clocks 
	 */
	outl( (i2s_rxcc_bcr_64x | i2s_rxcc_nbcg |i2s_rxcc_mstr | i2s_rxcc_rrel), I2SRxClkCfg );

	/* 
	 * Do an input to push the outl's past the wrapper 
	 */
	uiDEVCFG = inl(SYSCON_DEVCFG);
    
	DPRINTK("ep93xx_init_i2s_controller - EXIT\n");
}

/*
 * ep93xx_init_i2s_codec
 *
 * Note that codec must be getting i2s clocks for any codec
 * register writes to work.
 */
static void ep93xx_init_i2s_codec( void )
{
#if defined(CONFIG_ARCH_EDB9301) || defined(CONFIG_ARCH_EDB9302)
    unsigned int uiPADR, uiPADDR;
#endif
    
    DPRINTK("ep93xx_init_i2s_codec - enter\n");
    
	if( !audio_clocks_enabled_in_syscon )
	{
		DPRINTK("ep93xx_init_i2s_codec - EXIT - clocks not enabled\n");
		return;
	}

#ifdef CONFIG_CODEC_CS4271

	//
    // Some EDB9301 boards use EGPIO1 (port 1, bit 1) for the I2S reset.
    // EGPIO1 has a pulldown so if it isn't configured as an output, it
    // is low.
    //
#if defined(CONFIG_ARCH_EDB9301) || defined(CONFIG_ARCH_EDB9302)
	uiPADR  = inl(GPIO_PADR);
    uiPADDR = inl(GPIO_PADDR);
    
    // Clear bit 1 of the data register
    outl( (uiPADR & 0xfd), GPIO_PADR );
    uiPADR  = inl(GPIO_PADR);

    // Set bit 1 of the DDR to set it to output
    // Now we are driving the reset pin low.
    outl( (uiPADDR | 0x02), GPIO_PADDR );
    uiPADDR = inl(GPIO_PADDR);

    udelay( 2 );  /* plenty of time */

    // Set bit 1 of the data reg.  Now we drive the reset pin high.
    outl( (uiPADR | 0x02),  GPIO_PADR );
    uiPADR  = inl(GPIO_PADR);
#endif

    /*
     * Write to the control port, setting the enable control port bit
     * so that we can write to the control port.  OK?
     */
	SSPDriver->Write( SSP_Handle, 7, 0x02 );

    /*
     * Select slave, 24Bit I2S serial mode
     */
	SSPDriver->Write( SSP_Handle, 1, 0x01 );
	SSPDriver->Write( SSP_Handle, 6, 0x10 );

    /*
     * Set AMUTE (auto-mute) bit.
     */
	SSPDriver->Write( SSP_Handle, 2, 0x00 );

    /*
     * Set SOFT  bit.
     */
	//SSPDriver->Write( SSP_Handle, 3, 0x29 );

#else // CONFIG_CODEC_CS4228A

    /*
     * Nothing needs to happen here
     */

#endif
    
	DPRINTK("ep93xx_init_i2s_codec - EXIT\n");
}

#if 0
/*
 * ep93xx_mute_i2s_codec
 *
 * Note that codec must be getting i2s clocks for any codec
 * register writes to work.
 */
static void ep93xx_mute_i2s_codec( void )
{
	DPRINTK("ep93xx_mute_i2s_codec - enter\n");

	if( !audio_clocks_enabled_in_syscon )
	{
		DPRINTK("ep93xx_mute_i2s_codec - EXIT - clocks not enabled\n");
		return;
	}

#ifdef CONFIG_CODEC_CS4271

    /*
     * Set the driver's mute flag and use the set_volume routine
     * to write the current volume out with the mute bit set.
     */
    giDAC_Mute = 1;
    ep93xx_set_volume( 0, giOSS_Volume[0] );

#else // CONFIG_CODEC_CS4228A

    /*
     * Mute the MUTEC pin.
     */
	SSPDriver->Write( SSP_Handle, 5, 0x80 );

    /*
     * Mute the DACs
     */
	SSPDriver->Write( SSP_Handle, 4, 0xFC );
    
    giDAC_Mute = 1;

#endif

	DPRINTK("ep93xx_mute_i2s_codec - EXIT\n");
}
#endif

/*
 * ep93xx_automute_i2s_codec
 *
 * Note that codec must be getting i2s clocks for any codec
 * register writes to work.
 */
static void ep93xx_automute_i2s_codec( void )
{
	DPRINTK("ep93xx_automute_i2s_codec - enter\n");

	if( !audio_clocks_enabled_in_syscon )
	{
		DPRINTK("ep93xx_automute_i2s_codec - EXIT - clocks not enabled\n");
		return;
	}

#ifdef CONFIG_CODEC_CS4271

    /*
     * The automute bit is set by default for the CS4271.
     * Clear the driver's mute flag and use the set_volume routine
     * to write the current volume out with the mute bit cleared.
     */
    giDAC_Mute = 0;
    ep93xx_set_volume( 0, giOSS_Volume[0] );

#else // CONFIG_CODEC_CS4228A

    /*
     * Unmute the DACs
     */
	SSPDriver->Write( SSP_Handle, 4, 0 );

    /*
     * Unmute the MUTEC pin, turn on automute.
     */
	SSPDriver->Write( SSP_Handle, 5, 0x40 );
    
    giDAC_Mute = 0;
    
#endif

	DPRINTK("ep93xx_automute_i2s_codec - EXIT\n");
}

/*
 * Set the volume in the I2S DAC or ADC.
 * The channel parameter specifies which stereo channel in the
 * case of a 6 channel codec (channel 0, 1, or 2).
 */
static int ep93xx_set_volume( int channel, int iOSS_Volume )
{
    int iOSS_Volume_L, iOSS_Volume_R;
#ifdef CONFIG_CODEC_CS4271
	int iMute=0;
#endif
	
#ifdef CONFIG_CODEC_CS4271
	/* only one stereo channel on this codec... */
	if( channel > 0 )
		return 0;
#endif
    
	if( !audio_clocks_enabled_in_syscon )
	{
		return -EINVAL;
	}
    
	giOSS_Volume[channel] = iOSS_Volume;

    iOSS_Volume_L =  iOSS_Volume & 0x00ff;
    iOSS_Volume_R = (iOSS_Volume & 0xff00) >> 8;

	if (iOSS_Volume_L > 100)
		iOSS_Volume_L = 100;
	if (iOSS_Volume_L < 0)
		iOSS_Volume_L = 0;
	if (iOSS_Volume_R > 100)
		iOSS_Volume_R = 100;
	if (iOSS_Volume_R < 0)
		iOSS_Volume_R = 0;

#ifdef CONFIG_CODEC_CS4271
    /*
     * CS4271 volume is 0 to -127 dB in 1 dB increments.
     * So scale the volume so a range of 0 to 100 becomes 127 to 0.
     */
    giDAC_Volume_L[0] = 127 - ((iOSS_Volume_L * 127) / 100);
    giDAC_Volume_R[0] = 127 - ((iOSS_Volume_R * 127) / 100);

    if( giDAC_Mute )
    {
        iMute = 0x80;
    }

    /*
     * Write the volume for DAC1 and DAC2 (reg 4 and 5)
     */
	SSPDriver->Write( SSP_Handle, 4, (giDAC_Volume_L[0] | iMute) );
	SSPDriver->Write( SSP_Handle, 5, (giDAC_Volume_R[0] | iMute) );

#else // CONFIG_CODEC_CS4228A

	/*
	 * CS4228A DAC attenuation is 0 to 90.5 dB in 0.5 dB steps so scale
	 * volume so that a value of 0 to 100 is scaled to 181 to 0
	 */
    giDAC_Volume_L[channel] = 181 - ((iOSS_Volume_L * 181) / 100);
    giDAC_Volume_R[channel] = 181 - ((iOSS_Volume_R * 181) / 100);

    /*
     * Write the codec volume registers for DAC1 and DAC2 
	 * (reg 7,8 = ch0; reg 9,A = ch1; reg B,C = ch2)
     */
	SSPDriver->Write( SSP_Handle, 7 + (channel<<1), giDAC_Volume_L[channel] );
	SSPDriver->Write( SSP_Handle, 8 + (channel<<1), giDAC_Volume_R[channel] );
	
#endif

	return 0;
}

static int 
ep93xx_mixer_open(struct inode *inode, struct file *file)
{
	file->private_data = &ep93xx_i2s_hw;
	return 0;
}

static int 
ep93xx_mixer_release(struct inode *inode, struct file *file)
{
	return 0;
}

/* 
 * We can record, but we can't change the record input volume.
 * So we don't report that we can (we used to report that we can).
 * change to (SOUND_MASK_LINE) to show a record volume that is not settable
 * if your app needs this which it shouldn't
 */
#define REC_MASK	(0) // change to (SOUND_MASK_LINE) to show non settable record volume

#ifdef CONFIG_CODEC_CS4271
#define DEV_MASK	(REC_MASK | SOUND_MASK_VOLUME )
#else
#define DEV_MASK	(REC_MASK | SOUND_MASK_VOLUME | SOUND_MASK_LINE1 | SOUND_MASK_LINE2)
#endif

static int
ep93xx_mixer_ioctl(struct inode *inode, struct file *file, uint cmd, ulong arg)
{
	int val, nr, ret = 0;
	struct mixer_info mi;

	audio_hw_t * hw = (audio_hw_t *)file->private_data;

	DPRINTK("ep93xx_mixer_ioctl - enter.  IOC_TYPE is %c \n", _IOC_TYPE(cmd) );

	/*
	 * We only accept mixer (type 'M') ioctls.
	 */
	if (_IOC_TYPE(cmd) != 'M')
		return -EINVAL;

	if (cmd == SOUND_MIXER_INFO) 
	{
		DPRINTK("ep93xx_mixer_ioctl - SOUND_MIXER_INFO\n" );

#ifdef CONFIG_CODEC_CS4271
		strncpy(mi.id, "CS4271", sizeof(mi.id));
		strncpy(mi.name, "Cirrus CS4271", sizeof(mi.name));
#else // CONFIG_CODEC_CS4228A
		strncpy(mi.id, "CS4228A", sizeof(mi.id));
		strncpy(mi.name, "Cirrus CS4228A", sizeof(mi.name));
#endif
		mi.modify_counter = hw->modcnt;

		return copy_to_user( (void *)arg, &mi, sizeof(mi));
	}

	nr = _IOC_NR(cmd);

	if (_IOC_DIR(cmd) & _IOC_WRITE) 
	{
		ret = get_user(val, (int *)arg);
		if (ret)
			return ret;

		DPRINTK("ep93xx_mixer_ioctl write - nr is %d, val is 0x%04x\n", nr, val );
		
		switch (nr) 
		{
			case SOUND_MIXER_VOLUME:
				ret = ep93xx_set_volume( 0, val );
				hw->modcnt++;
				break;
				
			case SOUND_MIXER_LINE1:
				ret = ep93xx_set_volume( 1, val );
				hw->modcnt++;
				break;
				
			case SOUND_MIXER_LINE2:
				ret = ep93xx_set_volume( 2, val );
				hw->modcnt++;
				break;

			/*
			 * Can't actually set the record volume.  But
			 * if we return error, that might be bad.
			 */
#if REC_MASK
			case SOUND_MIXER_RECSRC:
				hw->modcnt++;
				break;
#endif
			default:
				ret = -EINVAL;
		}
	}

	if (ret == 0 && _IOC_DIR(cmd) & _IOC_READ) 
	{
		int nr = _IOC_NR(cmd);
		ret = 0;


		switch (nr) 
		{
			case SOUND_MIXER_VOLUME:     
        		DPRINTK("ep93xx_mixer_ioctl read - nr is %d (SOUND_MIXER_VOLUME)\n", nr );
				val = giOSS_Volume[0];
				break;

			case SOUND_MIXER_LINE1:     
        		DPRINTK("ep93xx_mixer_ioctl read - nr is %d (SOUND_MIXER_LINE1)\n", nr );
				val = giOSS_Volume[1];
				break;

			case SOUND_MIXER_LINE2:     
        		DPRINTK("ep93xx_mixer_ioctl read - nr is %d (SOUND_MIXER_LINE2)\n", nr );
				val = giOSS_Volume[2];
				break;
				
#if REC_MASK
		   	case SOUND_MIXER_LINE:       
        		DPRINTK("ep93xx_mixer_ioctl read - nr is %d (SOUND_MIXER_LINE)\n", nr );
				val = ((100<<8) | 100 );	
				break;
#endif
				
			case SOUND_MIXER_RECSRC:     
        		DPRINTK("ep93xx_mixer_ioctl read - nr is %d (SOUND_MIXER_RECSRC)\n", nr );
				val = REC_MASK;	
				break;
				
			case SOUND_MIXER_RECMASK:    
        		DPRINTK("ep93xx_mixer_ioctl read - nr is %d (SOUND_MIXER_RECMASK)\n", nr );
				val = REC_MASK;	
				break;
			
			case SOUND_MIXER_DEVMASK:    
        		DPRINTK("ep93xx_mixer_ioctl read - nr is %d (SOUND_MIXER_DEVMASK)\n", nr );
				val = DEV_MASK;	
				break;
			
			case SOUND_MIXER_CAPS:
        		DPRINTK("ep93xx_mixer_ioctl read - nr is %d (SOUND_MIXER_CAPS)\n", nr );
				val = SOUND_CAP_EXCL_INPUT;		
				break;

			case SOUND_MIXER_STEREODEVS: 
        		DPRINTK("ep93xx_mixer_ioctl read - nr is %d (SOUND_MIXER_STEREODEVS)\n", nr );
				val = 0;		
				break;

			default:	
        		DPRINTK("ep93xx_mixer_ioctl read - nr is %d (not handled, return 0)\n", nr );
				val = 0;     
				ret = -EINVAL;	
				break;
		}

		if (ret == 0)
			ret = put_user(val, (int *)arg);
	}

	return ret;
}

static struct file_operations ep93xx_mixer_fops = {
	owner:		THIS_MODULE,
	llseek:		no_llseek,
	ioctl:		ep93xx_mixer_ioctl,
	open:		ep93xx_mixer_open,
	release:	ep93xx_mixer_release,
};

/*
 * ep93xx_audio_init
 * Note that the codec powers up with its DAC's muted and
 * the serial format set to 24 bit I2S mode.
 */
static void ep93xx_audio_init(void *dummy)
{
    DPRINTK("ep93xx_audio_init - enter\n");
	
    /*
	 * Mute the DACs. Disable all audio channels.  
	 * Must do this to change sample rate.
	 */
    ep93xx_audio_disable_all();

	/*
	 * Set up the i2s clocks in syscon.  Enable them.
	 */ 
	i2s_output_stream0.sample_rate = AUDIO_SAMPLE_RATE_DEFAULT;
	ep93xx_set_samplerate( AUDIO_SAMPLE_RATE_DEFAULT );

	/*
	 * Set i2s' controller serial format, and enable 
	 * the i2s controller.
	 */
	ep93xx_init_i2s_controller();
	
    /*
     * Initialize codec serial format, etc.
     */
    ep93xx_init_i2s_codec();

	/*
	 * Clear the fifo and enable the tx0 channel.
	 */
	ep93xx_audio_enable_all();

	/*
	 * Set the volume for the first time.
	 */
	ep93xx_set_volume( 0, AUDIO_DEFAULT_VOLUME );
	ep93xx_set_volume( 1, AUDIO_DEFAULT_VOLUME );
	ep93xx_set_volume( 2, AUDIO_DEFAULT_VOLUME );

	
	/*
	 * Unmute the DAC and set the mute pin MUTEC to automute.
	 */
	ep93xx_automute_i2s_codec();
	
	DPRINTK("ep93xx_audio_init - EXIT\n");
}

/*
 * any_streams_are_active_or_stopped
 *
 * This routine returns 1 if either the input or output	streams is 
 * active or stopped.  If that is the case we are in the middle
 * of a transfer and don't want the sample rate to change.
 */
static __inline__ int any_streams_are_active_or_stopped( void )
{
	if( audio_state0.output_stream && audio_state0.wr_ref )
	{
		if( audio_state0.output_stream->active || audio_state0.output_stream->stopped )
		{
			return 1;
		}
	}
	if( audio_state0.input_stream && audio_state0.rd_ref )
	{
		if( audio_state0.input_stream->active || audio_state0.input_stream->stopped )
		{
			return 1;
		}
	}

#ifdef CONFIG_CODEC_CS4228A
	if( audio_state1.output_stream && audio_state1.wr_ref )
	{
		if( audio_state1.output_stream->active || audio_state1.output_stream->stopped )
		{
			return 1;
		}
	}
	if( audio_state1.input_stream && audio_state1.rd_ref )
	{
		if( audio_state1.input_stream->active || audio_state1.input_stream->stopped )
		{
			return 1;
		}
	}

	if( audio_state2.output_stream && audio_state2.wr_ref )
	{
		if( audio_state2.output_stream->active || audio_state2.output_stream->stopped )
		{
			return 1;
		}
	}
	if( audio_state2.input_stream && audio_state2.rd_ref )
	{
		if( audio_state2.input_stream->active || audio_state2.input_stream->stopped )
		{
			return 1;
		}
	}
#endif
	
	return 0;
}



static int ep93xx_audio_ioctl(struct inode *inode, struct file *file,
			     uint cmd, ulong arg)
{
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
			
			/*
			 * For I2S, sample rate is set for all channels, record and playback
			 * equally.  So we don't check to see if it's doing READ or WRITE.
			 */
			/*
			 * If we are already at this samplerate, don't change it.
			 */
			if( i2s_output_stream0.sample_rate == val )
			{
				return put_user( val, (long *) arg);
			}

			/*
			 * This driver won't allow changing the sample rate if we are 
			 * playing or recording audio on any channel.  This code saves
			 * the user from ugly sounds or silences.
			 */
			if( any_streams_are_active_or_stopped() )
			{
				printk("ep93xx-i2s: Audio driver can't change the sample rate while any\n");
				printk("other audio record or playback channel is being used.\n");
				return -EFAULT;
			}

		    /*
			 * Disable all audio channels.  Must do this to change sample rate.
			 */
		    ep93xx_audio_disable_all();
			ep93xx_set_samplerate( val );
			ep93xx_audio_enable_all();
	        
            /*
	         * We sorta should report the actual samplerate back to the calling
	         * application.  But some applications freak out if they don't get
	         * exactly what they asked for.  So we fudge and tell them what
	         * they want to hear.
	         */
			return put_user( val, (long *) arg);

		case SOUND_PCM_READ_RATE:
			DPRINTK("ep93xx_audio_ioctl - SOUND_PCM_READ_RATE\n");
			return put_user( i2s_output_stream0.sample_rate, (long *) arg);

		default:
			DPRINTK("ep93xx_audio_ioctl-->ep93xx_mixer_ioctl\n");
			/* Maybe this is meant for the mixer (As per OSS Docs) */
			return ep93xx_mixer_ioctl(inode, file, cmd, arg);
	}

	return ret;
}

static int ep93xx_audio_open0(struct inode *inode, struct file *file)
{
	DPRINTK( "ep93xx_audio_open0\n" );
	return ep93xx_audio_attach( inode, file, &audio_state0 );
}

#ifdef CONFIG_CODEC_CS4228A
static int ep93xx_audio_open1(struct inode *inode, struct file *file)
{
	DPRINTK( "ep93xx_audio_open1\n" );
	return ep93xx_audio_attach( inode, file, &audio_state1 );
}

static int ep93xx_audio_open2(struct inode *inode, struct file *file)
{
	DPRINTK( "ep93xx_audio_open2\n" );
	return ep93xx_audio_attach( inode, file, &audio_state2 );
}
#endif

/*
 * Missing fields of this structure will be patched with the call
 * to ep93xx_audio_attach().
 */
static struct file_operations ep93xx_audio_fops0 = 
{
	open:		ep93xx_audio_open0,
	owner:		THIS_MODULE
};

#ifdef CONFIG_CODEC_CS4228A
static struct file_operations ep93xx_audio_fops1 = 
{
	open:		ep93xx_audio_open1,
	owner:		THIS_MODULE
};
static struct file_operations ep93xx_audio_fops2 = 
{
	open:		ep93xx_audio_open2,
	owner:		THIS_MODULE
};
#endif

static int __init ep93xx_i2s_init(void)
{
	DPRINTK("ep93xx_i2s_init - enter\n");
	
    /*
     * Open an instance of the SSP driver for I2S codec register access.
     */
    SSP_Handle = SSPDriver->Open(I2S_CODEC,0);
    if( SSP_Handle < 1 )
    {
	    printk( "/drivers/sound/ep93xx-i2s.c: Couldn't open SSP driver!\n");
		return -ENODEV;
    }

	/*
	 * Enable audio early on, give the DAC time to come up.
	 */	
	ep93xx_audio_init( 0 );

	/* 
	 * Register devices using sound_core.c's devfs stuff
	*/
	audio_dev_id0 = register_sound_dsp(&ep93xx_audio_fops0, -1);
	if( audio_dev_id0 < 0 )
	{
		DPRINTK(" ep93xx_i2s_init: register_sound_dsp failed for dsp0.\n" );
		return -ENODEV;
	}
#ifdef CONFIG_CODEC_CS4228A
	audio_dev_id1 = register_sound_dsp(&ep93xx_audio_fops1, -1);
	if( audio_dev_id1 < 0 )
	{
		DPRINTK(" ep93xx_i2s_init: register_sound_dsp failed for dsp1.\n" );
		return -ENODEV;
	}
	audio_dev_id2 = register_sound_dsp(&ep93xx_audio_fops2, -1);
	if( audio_dev_id2 < 0 )
	{
		DPRINTK(" ep93xx_i2s_init: register_sound_dsp failed for dsp2.\n" );
		return -ENODEV;
	}
#endif
	
	mixer_dev_id = register_sound_mixer(&ep93xx_mixer_fops, -1);
	if( mixer_dev_id < 0 )
	{
		DPRINTK(" ep93xx_i2s_init: register_sound_dsp failed for mixer.\n" );
		return -ENODEV;
	}

	printk( KERN_INFO "EP93xx I2S audio support initialized.\n" );
	return 0;
}

static void __exit ep93xx_i2s_exit(void)
{
    SSPDriver->Close(SSP_Handle);
	
	unregister_sound_dsp( audio_dev_id0 );
	
#ifdef CONFIG_CODEC_CS4228A
	unregister_sound_dsp( audio_dev_id1 );
	unregister_sound_dsp( audio_dev_id2 );
#endif

	unregister_sound_mixer( mixer_dev_id );
}

module_init(ep93xx_i2s_init);
module_exit(ep93xx_i2s_exit);

MODULE_DESCRIPTION("Audio driver for the Cirrus EP93xx I2S controller.");

/* EXPORT_NO_SYMBOLS; */
