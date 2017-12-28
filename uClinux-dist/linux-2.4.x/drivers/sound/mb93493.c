/* mb93493-audio.c: description
 *
 * Copyright (C) 2004 Red Hat, Inc. All Rights Reserved.
 * Written by David Smith (dsmith@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/fujitsu/mb93493-i2s.h>
#include <linux/poll.h>
#include <linux/kmod.h>
#include <asm/semaphore.h>
#include <asm/pgalloc.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/dma.h>
#include <asm/mb93493-regs.h>
#include <linux/sound.h>
#include <linux/soundcard.h>
#include "../drivers/fujitsu/mb93493.h"

static int mb93493_audio_open(struct inode *inode, struct file *file)
{
    int err;

    err = mb93493_i2s_open(inode, file);
    return err;
}

static int mb93493_audio_release(struct inode *inode, struct file *file)
{
    int err;

    err = mb93493_i2s_release(inode, file);
    return err;
}

static int mb93493_audio_ioctl(struct inode *inode, struct file *file,
			       uint cmd, ulong arg)
{
    long val, val2;
    int ret = 0;
    int swap_bytes = 0;
    struct fr400i2s_config uconfig;

    switch (cmd) {
      case SNDCTL_DSP_STEREO:
	/* Set mono (0) or stereo (1) mode */
#ifdef DEBUG
	printk(KERN_INFO "mb93493_audio_ioctl: SNDCTL_DSP_STEREO\n");
#endif
	ret = get_user(val, (int *) arg);
	if (ret)
	{
	    return ret;
	}
	if (val < 0 || val > 1)
	{
	    return -EINVAL;
	}
#ifdef DEBUG
	printk(KERN_INFO "mb93493_audio_ioctl: SNDCTL_DSP_STEREO val %ld\n",
	       val);
#endif

	/* Get current config */
	ret = mb93493_i2s_ioctl_mem(inode, file, I2SIOCGCFG,
				    (unsigned long)&uconfig, 0);
	if (ret)
	{
	    return ret;
	}
	
	/* Set config */
	if (file->f_mode & FMODE_READ)
	{
	    uconfig.channel_n = (val + 1);
	}
	if (file->f_mode & FMODE_WRITE)
	{
	    uconfig.out_channel_n = (val + 1);
	}
	ret = mb93493_i2s_ioctl_mem(inode, file, I2SIOCSCFG,
				    (unsigned long)&uconfig, 0);
	break;

      case SOUND_PCM_READ_CHANNELS:
	/* returns the current number of channels, either 1 or 2 */
#ifdef DEBUG
	printk(KERN_INFO "mb93493_audio_ioctl: SNDCTL_PCM_READ_CHANNELS\n");
#endif
	/* Get current config */
	ret = mb93493_i2s_ioctl_mem(inode, file, I2SIOCGCFG,
				    (unsigned long)&uconfig, 0);
	if (ret)
	{
	    return ret;
	}
	
	if (file->f_mode & FMODE_READ)
	{
	    val = uconfig.channel_n;
	}
	else
	{
	    val = uconfig.out_channel_n;
	}
	ret = put_user(val, (long *)arg);
	break;

      case SOUND_PCM_WRITE_CHANNELS:	/* same as SNDCTL_DSP_CHANNELS */
	/* sets the number of channels: 1 for mono, 2 for stereo */
#ifdef DEBUG
	printk(KERN_INFO "mb93493_audio_ioctl: SNDCTL_PCM_WRITE_CHANNELS\n");
#endif
	ret = get_user(val, (int *) arg);
	if (ret)
	{
	    return ret;
	}
	if (val < 1 || val > 2)
	{
	    return -EINVAL;
	}

	/* Get current config */
	ret = mb93493_i2s_ioctl_mem(inode, file, I2SIOCGCFG,
				    (unsigned long)&uconfig, 0);
	if (ret)
	{
	    return ret;
	}
	
	/* Set config */
	if (file->f_mode & FMODE_READ)
	{
	    uconfig.channel_n = val;
	}
	if (file->f_mode & FMODE_WRITE)
	{
	    uconfig.out_channel_n = val;
	}
	ret = mb93493_i2s_ioctl_mem(inode, file, I2SIOCSCFG,
				    (unsigned long)&uconfig, 0);
	break;

      case SNDCTL_DSP_GETFMTS:
#ifdef DEBUG
	printk(KERN_INFO "mb93493_audio_ioctl: SNDCTL_DSP_GETFMTS\n");
#endif
	val = AFMT_S16_BE | AFMT_U16_BE | AFMT_S16_LE | AFMT_U16_LE;
	ret = put_user(val, (long * )arg);
	break;

      case SNDCTL_DSP_SETFMT:
#ifdef DEBUG
	printk(KERN_INFO "mb93493_audio_ioctl: SNDCTL_DSP_SETFMT\n");
#endif
	ret = get_user(val, (int *) arg);
	if (ret)
	{
	    printk(KERN_INFO
		   "mb93493_audio_ioctl: get_user failed with %d\n",
		   ret);
	    return ret;
	}

	/* Try to handle formats */
	switch (val)
	{
	  case AFMT_S16_BE:		/* Big-endian variant of
					 * signed 16-bit encoding */
	  case AFMT_U16_BE:		/* Unsigned big-endian 16-bit
					 * encoding */
#ifdef DEBUG
	    printk(KERN_INFO "mb93493_audio_ioctl: setting 16-bit mode (%s)\n",
		   ((val == AFMT_S16_BE) ? "AFMT_S16_BE"
		    : ((val == AFMT_U16_BE) ? "AFMT_U16_BE"
		       : "unknown")));
#endif
	    val = -16;
	    val2 = 32;			/* for input, either 8 or 32 works */
	    ret = 0;
	    break;

	  case AFMT_S16_LE:		/* Standard signed 16-bit
					 * little-endian encoding */
	  case AFMT_U16_LE:		/* Unsigned little-endian
					 * 16-bit encoding */
#ifdef DEBUG
	    printk(KERN_INFO "mb93493_audio_ioctl: setting 16-bit mode (%s)\n",
		   ((val == AFMT_S16_LE) ? "AFMT_S16_LE"
		    : ((val == AFMT_U16_LE) ? "AFMT_U16_LE"
		       : "unknown")));
#endif
	    val = -16;
	    val2 = 32;			/* for input, either 8 or 32 works */
	    swap_bytes = 1;
	    ret = 0;
	    break;


	  case AFMT_U8:			/* Standard unsigned 8-bit encoding */
	  case AFMT_S8:			/* Signed 8-bit encoding */
	  case AFMT_MU_LAW:		/* Logarithmic mu-law encoding */
	  case AFMT_A_LAW:		/* Logarithmic A-law encoding */
	  case AFMT_IMA_ADPCM:		/* Interactive Multimedia
					 * Association (IMA) ADPCM encoding */
	  case AFMT_MPEG:		/* MPEG 2 encoding */
	  case AFMT_AC3:		/* Dolby Digital AC3 encoding */
	  default:
	    printk(KERN_INFO "mb93493_audio_ioctl: bad mode %ld\n", val);
	    ret = -EINVAL;
	    break;
	}

	if (! ret)
	{
	    /* Get current config */
	    ret = mb93493_i2s_ioctl_mem(inode, file, I2SIOCGCFG,
					(unsigned long)&uconfig, 0);
	    if (ret)
	    {
		printk(KERN_INFO
		       "mb93493_audio_ioctl: I2SIOCGCFG failed with %d\n",
		       ret);
		return ret;
	    }
	
	    /* Set config */
	    if (file->f_mode & FMODE_READ)
	    {
		uconfig.bit = val2;
		uconfig.ami = 1;
	    }
	    if (file->f_mode & FMODE_WRITE)
	    {
		uconfig.out_bit = val;
		uconfig.amo = 1;
		uconfig.out_swap_bytes = swap_bytes;
	    }
	    uconfig.fl = 0;
	    uconfig.fs = 0;
	    ret = mb93493_i2s_ioctl_mem(inode, file, I2SIOCSCFG,
					(unsigned long)&uconfig, 0);
	    if (ret)
	    {
		printk(KERN_INFO
		       "mb93493_audio_ioctl: I2SIOCSCFG failed with %d\n",
		       ret);
	    }
	}
	break;

      case SNDCTL_DSP_SPEED:
#ifdef DEBUG
	printk(KERN_INFO "mb93493_audio_ioctl: SNDCTL_DSP_SPEED\n");
#endif
	ret = get_user(val, (int *) arg);
	if (ret)
	{
	    printk(KERN_INFO
		   "mb93493_audio_ioctl: get_user failed with %d\n",
		   ret);
	    return ret;
	}

	/* Get current config */
	ret = mb93493_i2s_ioctl_mem(inode, file, I2SIOCGCFG,
				    (unsigned long)&uconfig, 0);
	if (ret)
	{
	    return ret;
	}

	/* Make sure we handle frequency */
	switch (val)
	{
	  case 8000:
#ifdef DEBUG
	    printk(KERN_INFO "mb93493_audio_ioctl: SNDCTL_DSP_SPEED 8K\n");
#endif
#ifdef CONFIG_MB93093_PDK
	    __set_FPGATR_AUDIO_CLK(MB93093_FPGA_FPGATR_AUDIO_CLK_02MHz);
#endif
	    uconfig.div = 2;
	    break;

	  case 16000:
#ifdef DEBUG
	    printk(KERN_INFO "mb93493_audio_ioctl: SNDCTL_DSP_SPEED 16K\n");
#endif
#ifdef CONFIG_MB93093_PDK
	    __set_FPGATR_AUDIO_CLK(MB93093_FPGA_FPGATR_AUDIO_CLK_02MHz);
#endif
	    uconfig.div = 0;
	    break;

	  case 32000:
#ifdef DEBUG
	    printk(KERN_INFO "mb93493_audio_ioctl: SNDCTL_DSP_SPEED 32K\n");
#endif
#ifdef CONFIG_MB93093_PDK
	    __set_FPGATR_AUDIO_CLK(MB93093_FPGA_FPGATR_AUDIO_CLK_12MHz);
#endif
	    uconfig.div = 3;
	    break;

	  case 44100:
#ifdef DEBUG
	    printk(KERN_INFO "mb93493_audio_ioctl: SNDCTL_DSP_SPEED 44.1K\n");
#endif
#ifdef CONFIG_MB93093_PDK
	    __set_FPGATR_AUDIO_CLK(MB93093_FPGA_FPGATR_AUDIO_CLK_11MHz);
#endif
	    uconfig.div = 2;
	    break;

	  case 48000:
#ifdef DEBUG
	    printk(KERN_INFO "mb93493_audio_ioctl: SNDCTL_DSP_SPEED 48K\n");
#endif
#ifdef CONFIG_MB93093_PDK
	    __set_FPGATR_AUDIO_CLK(MB93093_FPGA_FPGATR_AUDIO_CLK_12MHz);
#endif
	    uconfig.div = 2;
	    break;

	  default:
	    printk(KERN_INFO "mb93493_audio_ioctl: bad speed %ld\n", val);
	    ret = -EINVAL;
	    break;
	}

	if (ret == 0) {
	    ret = mb93493_i2s_ioctl_mem(inode, file, I2SIOCSCFG,
					(unsigned long)&uconfig, 0);
	}
	break;

      case SNDCTL_DSP_SYNC:
#ifdef DEBUG
	printk(KERN_INFO "mb93493_audio_ioctl: SNDCTL_DSP_SYNC\n");
#endif
	ret = mb93493_i2s_ioctl_mem(inode, file, I2SIOC_OUT_DRAIN, 0, 0);
	if (ret)
	{
	    printk(KERN_INFO
		   "mb93493_audio_ioctl: I2SIOC_OUT_DRAIN failed with %d\n",
		   ret);
	}
	break;

      case SNDCTL_DSP_RESET:
#ifdef DEBUG
	printk(KERN_INFO "mb93493_audio_iotcl: SNDCTL_DSP_RESET\n");
#endif
	ret = mb93493_i2s_ioctl_mem(inode, file, I2SIOCSTOP, 0, 0);
	if (ret)
	{
	    printk(KERN_INFO
		   "mb93493_audio_ioctl: I2SIOCSTOP failed with %d\n",
		   ret);
	}
	else
	{
	    ret = mb93493_i2s_ioctl_mem(inode, file, I2SIOC_OUT_STOP, 0, 0);
	    if (ret)
	    {
		printk(KERN_INFO
		       "mb93493_audio_ioctl: I2SIOC_OUT_STOP failed with %d\n",
		       ret);
	    }
	}
	break;

      case SNDCTL_DSP_GETBLKSIZE:
#ifdef DEBUG
	printk(KERN_INFO "mb93493_audio_iotcl: SNDCTL_DSP_GETBLKSIZE\n");
#endif
	/* Get current config */
	ret = mb93493_i2s_ioctl_mem(inode, file, I2SIOCGCFG,
				    (unsigned long)&uconfig, 0);
	if (ret)
	{
	    return ret;
	}
	val = uconfig.out_buf_unit_sz;
	ret = put_user(val, (long *)arg);
	break;

      case TCGETS:
#ifdef DEBUG
	printk(KERN_INFO "mb93493_audio_iotcl: TCGETS\n");
#endif
	ret = -EINVAL;
	break;

      default:
	printk(KERN_INFO
	       "mb93493_audio_ioctl: got unhandled %u ioctl\n", cmd);
	ret = -EINVAL;
	break;
    }
    return ret;
}

static ssize_t mb93493_audio_write(struct file *file, const char *buffer,
				   size_t count, loff_t *ppos)
{
    ssize_t sz;

    sz = mb93493_i2s_write(file, buffer, count, ppos);
    return sz;
}

static ssize_t mb93493_audio_read(struct file *file, char *buffer,
				  size_t count, loff_t *ppos)
{
    ssize_t sz;

    sz = mb93493_i2s_read_data(file, buffer, count, ppos);
    return sz;
}


static struct file_operations mb93493_audio_fops = {
	.owner		= THIS_MODULE,
	.open		= mb93493_audio_open,
	.release	= mb93493_audio_release,
	.ioctl		= mb93493_audio_ioctl,
	.read		= mb93493_audio_read,
	.write		= mb93493_audio_write,
};

static int audio_dev_id;
//static int mixer_dev_id;

static int __init mb93493_audio_init(void)
{
    /* register devices */
    audio_dev_id = register_sound_dsp(&mb93493_audio_fops, -1);
    //mixer_dev_id = register_sound_mixer(?, -1);

    /* bring in i2s support */
    request_module("fr400i2s");

    printk(KERN_INFO
	   "Fujitsu MB93493 audio support initialized on device %d\n",
	   audio_dev_id);
    return 0;
}

static void __exit mb93493_audio_exit(void)
{
    unregister_sound_dsp(audio_dev_id);
}

module_init(mb93493_audio_init);
module_exit(mb93493_audio_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Fujitsu MB93493 Audio Driver");
MODULE_AUTHOR("David Smith <dsmith@redhat.com>");

EXPORT_NO_SYMBOLS;
