/*****************************************************************************
 *
 *   Copyright (C) Arcturus Networks Inc. 
 *                 by Phil Wilshire <philwil@uclinux.org>
 *
 *   Set up the 68VZ328 LCD buffer
 *
 * usage instructions
   cd /mnt/arcturus_graphics/uClinux-dist/modules/lcd 
   rmmod lcd_module ; insmod lcd_module.o 
   mknod fb0 c 29 0
   insmod fbcon-mfb.o 

   insmod  mc68x328fb.o    

   ../../user/lissa/lissa
 *****************************************************************************/

#include <linux/module.h>
#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <asm/MC68VZ328.h>

#define LCD_BASE  0xFFFFFA00
#define PIX_ROWS  320
#define PIX_COLS  240/8

//#include "bootlogo.h"

#define DEBUG 0

#define dbgprintk if (DEBUG) printk

unsigned char pixbuf[PIX_ROWS * PIX_COLS];

/*
 * these asm functions also need to be done somewhere
	moveb	#0xff,   0xfffff443	PKSEL
                #0xff   PSW value
	moveb	#0xff,   0xfffff440	PKDIR
                #0x7f  PSW value   
 * not correct since pk7(LD7) is the touch_busy an needs to be a GPIO input
 * pk6(LD6) is touch_cs and needs to be a GPIO output        
	moveb	#0x00,   0xfffff413	PCSEL
        moveb	#0xdf,   0xfffff441	PKDATA
 *
 */

/*
 *      Define the LCD register set addresses.
 */

typedef struct VZlcd_s {
        unsigned long  lssa;       /* Screen Starting Address Register*/
        unsigned char  dummy1;     /* dummy register */
        unsigned char  lvpw;       /* Virtual Page WIDTH */
        unsigned char  dummy2[2];  /* dummy register2 */
        unsigned short lxmax;      /* Screen Width */
        unsigned short lymax;      /* Screen Height */
        unsigned short dummy3[6];  /* dummy register3 */
        unsigned short lcxp;       /* Cursor X Position */
        unsigned short lcyp;       /* Cursor X Position */
        unsigned short lcwch;      /* Cursor Width (12-8) Height (4-0) */
        unsigned char  dummy4;     /* dummy register */
        unsigned char  lblkc;      /* Cursor Blink */
        unsigned char  lpicf;      /* Panel Interface Config Reg  */
        unsigned char  lpolcf;     /* Panel Interface Config Reg  */
        unsigned char  dummy5;     /* dummy register */
        unsigned char  lacdrc;     /* Rate Control Reg  */
        unsigned char  dummy6;     /* dummy register */
        unsigned char  lpxcd;      /* Pixel Clock Divide Reg  */
        unsigned char  dummy7;     /* dummy register */
        unsigned char  lckcon;     /* LCD  Clock Control Reg  */
        unsigned char  lrra;       /* LCD  Refresh Rate Adj Reg  */
        unsigned char  dummy8[4];  /* dummy register */
        unsigned char  lposr;      /* LCD  Panning Offset Reg  */
        unsigned char  dummy9[5];  /* dummy register */
        unsigned char  lgpmr;      /* LCD  Grey Palette Mapping  Reg  */
        unsigned char  dummy10[2]; /* dummy register */
        unsigned short pwmr;       /* LCD  Contrast Control  Reg  */
        unsigned char  rmcr;       /* LCD  Refresh Mode Control  Reg  */
        unsigned char  dmacr;      /* LCD  DMA Control  Reg a39 */
} VZlcd_t __attribute__((packed));
 
static unsigned long  old_lssa;
static  VZlcd_t * mylcd = (VZlcd_t * ) LCD_BASE;
static unsigned int  lvpw = 0x3c;
static unsigned int old_lvpw = 0x3c;
MODULE_PARM(lvpw,"i");

static int __init clear_pixbuf(void)
{
  int i,j,k;
  unsigned char pval;
  int mycols = 240/8;
  int myrows = 320;

  /* clear the buffer */
  for ( j = 0 ; j < (PIX_ROWS); j++ ) {
     for ( i = 0 ; i < (PIX_COLS) ; i++ ) {
      pixbuf[(j* PIX_COLS) + i] = 0x00;
    }
  }

  /* draw a couple of lines  ROWS = 480 cols = 640 /8 wrong */
  for ( j = 0 ; j < myrows; j++ ) {
    pval = (unsigned char)0xff;

#if 0
    if (j < 10 ) {
      for ( k = 0 ; k < 20 ; k++ ) {
	pixbuf[(j* mycols) + k] = pval;
      }
    }
    if ((j>10) && (j < 20 )) {
      for ( k = 20 ; k < 30 ; k++ ) {
	pixbuf[(j* mycols) + k] = pval;
      }
    }
    if ((j>300) && (j < 320 )) {
      for ( k = 20 ; k < 30 ; k++ ) {
	pixbuf[(j* mycols) + k] = pval;
      }
    }
#endif

    if (j == 11 ) {
      for ( k = 0 ; k < 120 ; k++ ) {
	pixbuf[(j* mycols) + k] = pval;
      }
    }
#if 0
    if (j == 11 ) {
      for ( k = 0 ; k < 120 ; k++ ) {
	pixbuf[(j* mycols) + k] = pval;
      }
    }
    if ((j > 15 ) && ( j < 45)) {
      for ( k = 40 ; k < 80 ; k++ ) {
	pixbuf[(j* mycols) + k] = pval;
      }
    }

    if ((j > 50 ) && ( j < 80)) {
      for ( k = 40 ; k < 80 ; k++ ) {
	pixbuf[(j* mycols) + k] = 0x80;
      }
    }

    if (j > myrows-10 ) {
      for ( k = 100 ; k < 120 ; k++ ) {
	pixbuf[(j* mycols) + k] = pval;
      }
    }
#endif
  }

  return 0;
}

static int __init test_module_init(void)
{
	printk("Test Module init\n");
	dbgprintk("size of pixbuf(0X%p) = %ld \n", &pixbuf,sizeof(pixbuf));
	dbgprintk("address of dmacr (a39) = 0x%p \n", &mylcd->dmacr);
	dbgprintk("value of lssa = 0x%x \n", (unsigned int)mylcd->lssa);

        PKSEL = 0xff;  /* select GPIO Functions */
        PKDIR = 0xff;  /* select all 0-6 out 7 as input */

        PCSEL = 0x00;  /* select dedicated  LCD functions */
 
	old_lssa= mylcd->lssa;
	clear_pixbuf();

        old_lvpw = (unsigned int) mylcd->lvpw; 
        mylcd->lvpw = (unsigned char )lvpw;	

	dbgprintk("value of lvpw was 0x%x is now 0x%x \n", 
	       old_lvpw,(unsigned char)mylcd->lvpw);
	dbgprintk("value of lxmax was 0x%x  at 0x%x\n", 
	       (unsigned char)mylcd->lxmax,
	       (int)&mylcd->lxmax);
	dbgprintk("value of lpolcf was 0x%x  at 0x%x\n", 
	       (unsigned char)mylcd->lpolcf,
	       (int)&mylcd->lpolcf);

	dbgprintk("value of lposr was 0x%x  at 0x%x\n", 
	       (unsigned char)mylcd->lposr,
	       (int)&mylcd->lposr);

	mylcd->lvpw   = (unsigned char) 0x0f;
	mylcd->lxmax  = (unsigned short)0x00f0;
	mylcd->lymax  = (unsigned short)0x013f;
	mylcd->lpicf  = (unsigned char) 0x08;
	mylcd->lpolcf = (unsigned char) 0x00;
	mylcd->lacdrc = (unsigned char) 0x00;
	mylcd->lpxcd  = (unsigned char) 0x03;
	mylcd->lckcon = (unsigned char) 0x82;  
	mylcd->lssa   = (unsigned long)pixbuf;

        PKDATA = 0xdf;  /* Keep CS high for now */

	return(0);
}

void test_module_fini(void)
{
	printk("Test Module finished\n");
        mylcd->lvpw = (unsigned char)old_lvpw;	
	mylcd->lssa=old_lssa;
}

/*****************************************************************************/

module_init(test_module_init);
module_exit(test_module_fini);

/*****************************************************************************/
MODULE_LICENSE("GPL");
