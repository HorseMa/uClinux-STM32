/*
 * Copyright c                Realtek Semiconductor Corporation, 2003
 * All rights reserved.                                                    
 * 
 * $Author: davidm $
 *   rtl865x_proc.c: /proc filesystem interface of RTL865x
 *
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <asm/uaccess.h>

#include "rtl865x/rtl_glue.h" /*rtlglue_printf( )*/

#define PROC_NAME "rtl865x"

#define REG32(reg)	(*(volatile unsigned int *)(reg))
#define REG16(reg)	(*(volatile unsigned short *)(reg))
#define REG8(reg)	(*(volatile unsigned char *)(reg))

static struct proc_dir_entry 
	*rtl865x_proc, *reg32_proc, *reg16_proc, *reg8_proc;
static unsigned int _reg32,_reg16,_reg8;
static struct proc_dir_entry 
	*mem32_proc, *mem16_proc, *mem8_proc;

#define TMPSIZE 80
static char tmp[TMPSIZE];

static int _read_reg32(char *page, char **start,
			     off_t off, int count,
			     int *eof, void *data){
	int len;
	//rtlglue_printf("%s: %u=%08x\n", __FUNCTION__, _reg32,REG32(_reg32) );
	len = sprintf(page, "0x%08x\n", REG32(_reg32));
	return len;
}

static int _read_reg16(char *page, char **start,
			     off_t off, int count,
			     int *eof, void *data){
	int len;
	//rtlglue_printf("%s: %u=%04x\n", __FUNCTION__, _reg16,REG16(_reg16) );
	len = sprintf(page, "0x%04x\n", (unsigned short)REG16(_reg16));
	return len;
}

static int _read_reg8(char *page, char **start,
			     off_t off, int count,
			     int *eof, void *data){
	int len;
	//rtlglue_printf("%s: %u=%02x\n", __FUNCTION__, _reg8,REG8(_reg8) );
	len = sprintf(page, "0x%02x\n", (unsigned char) REG8(_reg8));
	return len;
}


static int _write_reg32(struct file *file,
			     const char *buffer,
			     unsigned long count, 
			     void *data){
	if(copy_from_user(tmp, buffer, strlen(buffer)))
		return -EFAULT;
	_reg32=simple_strtoul(tmp, NULL,0);
	//rtlglue_printf("%s: %u\n", __FUNCTION__, _reg32);
	return strlen(buffer);
}

static int _write_reg16(struct file *file,
			     const char *buffer,
			     unsigned long count, 
			     void *data){
	if(copy_from_user(tmp, buffer, strlen(buffer)))
		return -EFAULT;
	_reg16=simple_strtoul(tmp, NULL,0);
	//rtlglue_printf("%s: %u\n", __FUNCTION__, _reg16);
	return strlen(buffer);
}

static int _write_reg8(struct file *file,
			     const char *buffer,
			     unsigned long count, 
			     void *data){
	if(copy_from_user(tmp, buffer, strlen(buffer)))
		return -EFAULT;
	_reg8=simple_strtoul(tmp, NULL,0);
	//rtlglue_printf("%s: %u\n", __FUNCTION__, _reg8);
	return strlen(buffer);
}


static int _write_mem32(struct file *file,
			     const char *buffer,
			     unsigned long count, 
			     void *data){
	if(copy_from_user(tmp, buffer, strlen(buffer)))
		return -EFAULT;
	tmp[10]='\0';
	REG32(_reg32)=simple_strtoul(tmp, NULL,0);
	rtlglue_printf("%s: %u\n", tmp, REG32(_reg32));
	return strlen(buffer);
}

static int _write_mem16(struct file *file,
			     const char *buffer,
			     unsigned long count, 
			     void *data){
	if(copy_from_user(tmp, buffer, strlen(buffer)))
		return -EFAULT;
	tmp[6]='\0';
	REG16(_reg16)=(unsigned short)simple_strtoul(tmp, NULL,0);
	rtlglue_printf("%s: %u\n", tmp, REG16(_reg16));
	return strlen(buffer);
}

static int _write_mem8(struct file *file,
			     const char *buffer,
			     unsigned long count, 
			     void *data){
	if(copy_from_user(tmp, buffer, strlen(buffer)))
		return -EFAULT;
	tmp[4]='\0';
	REG8(_reg8)=(unsigned char)simple_strtoul(tmp, NULL,0);
	rtlglue_printf("%s: %u\n", tmp, REG8(_reg8));
	return strlen(buffer);
}



int rtl865x_init_procfs(void)
{
	int rv = 0;

	/* create directory */
	rtl865x_proc= proc_mkdir(PROC_NAME, NULL);
	if(rtl865x_proc == NULL) {
		rv = -ENOMEM;
		goto out;
	}
	
	reg32_proc = create_proc_entry("reg32", 0644, rtl865x_proc);
	if(reg32_proc == NULL) {
		rv = -ENOMEM;
		goto reg32_out;
	}
	reg32_proc->data = reg32_proc->owner = NULL;
	reg32_proc->read_proc = _read_reg32;
	reg32_proc->write_proc = _write_reg32;

	reg16_proc = create_proc_entry("reg16", 0644, rtl865x_proc);
	if(reg16_proc == NULL) {
		rv = -ENOMEM;
		goto reg16_out;
	}
	reg16_proc->data = reg16_proc->owner = NULL;
	reg16_proc->read_proc = _read_reg16;
	reg16_proc->write_proc = _write_reg16;

	reg8_proc = create_proc_entry("reg8", 0644, rtl865x_proc);
	if(reg8_proc == NULL) {
		rv = -ENOMEM;
		goto reg8_out;
	}	
	reg8_proc->data = reg8_proc->owner = NULL;
	reg8_proc->read_proc = _read_reg8;
	reg8_proc->write_proc = _write_reg8;

	
	mem32_proc = create_proc_entry("mem32", 0644, rtl865x_proc);
	if(mem32_proc == NULL) {
		rv = -ENOMEM;
		goto mem32_out;
	}
	mem32_proc->data = mem32_proc->owner = NULL;
	mem32_proc->read_proc = NULL;
	mem32_proc->write_proc = _write_mem32;

	mem16_proc = create_proc_entry("mem16", 0644, rtl865x_proc);
	if(mem16_proc == NULL) {
		rv = -ENOMEM;
		goto mem16_out;
	}
	mem16_proc->data = mem16_proc->owner = NULL;
	mem16_proc->read_proc = NULL;
	mem16_proc->write_proc = _write_mem16;

	mem8_proc = create_proc_entry("mem8", 0644, rtl865x_proc);
	if(mem8_proc == NULL) {
		rv = -ENOMEM;
		goto mem8_out;
	}	
	mem8_proc->data = mem8_proc->owner = NULL;
	mem8_proc->read_proc = NULL;
	mem8_proc->write_proc = _write_mem8;


	/* everything OK */
	//rtlglue_printf("proc entry '%s' initialised\n",PROC_NAME);
	return 0;

//should be in reverse order
mem8_out:
	remove_proc_entry("mem8", rtl865x_proc);
mem16_out:
	remove_proc_entry("mem16", rtl865x_proc);
mem32_out:
	remove_proc_entry("mem32", rtl865x_proc);
reg8_out:
	remove_proc_entry("reg8", rtl865x_proc);
reg16_out:
	remove_proc_entry("reg16", rtl865x_proc);
reg32_out:
	remove_proc_entry("reg32", rtl865x_proc);
out:
	return rv;
}


void rtl865x_cleanup_procfs(void)
{
	remove_proc_entry("reg32", rtl865x_proc);
	remove_proc_entry("reg16", rtl865x_proc);
	remove_proc_entry("reg8", rtl865x_proc);
	remove_proc_entry("mem32", rtl865x_proc);
	remove_proc_entry("mem16", rtl865x_proc);
	remove_proc_entry("mem8", rtl865x_proc);
	remove_proc_entry(PROC_NAME, NULL);

	rtlglue_printf("proc entry '%s' removed\n",PROC_NAME);
}

