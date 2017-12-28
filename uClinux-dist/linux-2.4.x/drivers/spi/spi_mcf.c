/**
*******************************************************************************
*
* \brief	spi_mcf.c - Motorola QSPI device driver
*
* (C) Copyright 2004, Josef Baumgartner (josef.baumgartner@telex.de)
*
* Based on source code mcf_qspi.c from Wayne Roberts (wroberts1@home.com)
*
*
* This program is free software; you can redistribute it and/or modify it
* under the terms of the GNU General Public License as published by the Free
* Software Foundation; either version 2 of the License, or (at your option)
* any later version.
*
* This program is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
* more details.
*
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 675 Mass Ave, Cambridge, MA 02139, USA.
*
******************************************************************************/

/*****************************************************************************/
/* includes */
#include <asm/coldfire.h>               /* gets us MCF_MBAR value */
#include <asm/mcfsim.h>                 /* MCFSIM offsets */
#include <asm/semaphore.h>
#include <asm/system.h>                 /* cli() and friends */
#include <asm/uaccess.h>
#include <linux/config.h>
#include <linux/devfs_fs_kernel.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/wait.h>

#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0)) || \
     (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,99)))
#error Driver only supports kernel 2.4.x
#endif

/* Include versioning info, if needed */
#if (defined(MODULE) && defined(CONFIG_MODVERSIONS) && !defined(MODVERSIONS))
#define MODVERSIONS
#endif

#if defined(MODVERSIONS)
#include <linux/modversions.h>
#endif

#include "spi_mcf.h"

#define MCF_QSPI_VERSION	"1.0"


/* struct wait_queue *wqueue; */
static DECLARE_WAIT_QUEUE_HEAD(wqueue);   /* use ver 2.4 static declaration - ron */
/* or should we use   wait_queue_heat_t *wqueue   ?? see page 141  */

static unsigned char dbuf[1024];

/*  static struct semaphore sem = MUTEX;   */
static DECLARE_MUTEX(sem);


/**
*******************************************************************************
* \brief        coldfire qspi interrupt routine
*
******************************************************************************/
static void spi_dev_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
        u16 qir = (QIR & (QIR_WCEF | QIR_ABRT | QIR_SPIF));

        /* Check write collision and transfer abort flags.  Report any
         * goofiness. */
        if (qir & QIR_WCEF)
                printk(KERN_INFO "%s: WCEF\n", __FILE__);

        if (qir & QIR_ABRT)
                printk(KERN_INFO "%s: ABRT\n", __FILE__);

        /* Check for completed transfer.  Wake any tasks sleeping on our
         * global wait queue. */
        if (qir & QIR_SPIF)
                wake_up(&wqueue);

        /* Clear any set flags. */
        QIR |= qir;
}

/**
*******************************************************************************
* \brief        coldfire qspi ioctls
*
******************************************************************************/
int spi_dev_ioctl(void *device, unsigned int cmd, unsigned long arg)
{
        int ret = 0;
        struct qspi_read_data *read_data;
        qspi_dev *dev = device;
        int error;

#if 0
        printk ("------------------------------------"
                "\nDevice Number: %d\n", dev->num);
        printk ("dev->dohie = %d\n", dev->dohie);
        printk ("dev->bits = %d\n", dev->bits);
        printk ("dev->cpol = %d\n", dev->cpol);
        printk ("dev->cpha = %d\n", dev->cpha);
        printk ("dev->baud = %d\n", dev->baud);
        printk ("dev->qcd = %d\n", dev->qcd);
        printk ("dev->dtl = %d\n", dev->dtl);
        printk ("dev->qcr_cont = %d\n", dev->qcr_cont);
        printk ("dev->dsp_mod = %d\n", dev->dsp_mod);
        printk ("dev->odd_mod = %d\n", dev->odd_mod);
        printk ("dev->poll_mod = %d\n\n", dev->poll_mod);
#endif

        down(&sem);

        switch (cmd) {
                /* Set QMR[DOHIE] (high-z Dout between transfers) */
                case QSPIIOCS_DOUT_HIZ:
                        dev->dohie = (arg ? 1 : 0);
                        break;

                /* Set QMR[BITS] */
                case QSPIIOCS_BITS:
                        if (((arg > 0) && (arg < 8)) || (arg > 16)) {
                                ret = -EINVAL;
                                break;
                        }

                        dev->bits = (u8)arg;
                        break;

                /* Get QMR[BITS] */
                case QSPIIOCG_BITS:
                        *((int *)arg) = dev->bits;
                        break;

                /* Set QMR[CPOL] (QSPI_CLK inactive state) */
                case QSPIIOCS_CPOL:
                        dev->cpol = (arg ? 1 : 0);
                        break;

                /* Set QMR[CPHA] (QSPI_CLK phase, 1 = rising edge) */
                case QSPIIOCS_CPHA:
                        dev->cpha = (arg ? 1 : 0);
                        break;

                /* Set QMR[BAUD] (QSPI_CLK baud rate divisor) */
                case QSPIIOCS_BAUD:
                        if (arg > 255) {
                                ret = -EINVAL;
                                break;
                        }

                        dev->baud = (u8)arg;
                        break;

                /* Set QDR[QCD] (QSPI_CS to QSPI_CLK setup) */
                case QSPIIOCS_QCD:
                        if (arg > 127) {
                                ret = -EINVAL;
                                break;
                        }

                        dev->qcd = (u8)arg;
                        break;

                /* Set QDR[DTL] (QSPI_CLK to QSPI_CS hold) */
                case QSPIIOCS_DTL:
                        if (arg > 255) {
                                ret = -EINVAL;
                                break;
                        }

                        dev->dtl = (u8)arg;
                        break;

                /* Set QCRn[CONT] (QSPI_CS continuous mode, 1 = remain
                 * asserted after transfer of 16 data words) */
                case QSPIIOCS_CONT:
                        dev->qcr_cont = (arg ? 1 : 0);
                        break;

                /* Set DSP mode, used to limit transfers to 15 bytes for
                 * 24-bit DSPs */
                case QSPIIOCS_DSP_MOD:
                        dev->dsp_mod  = (arg ? 1 : 0);
                        break;

                /* If an odd count of bytes is transferred, force the transfer
                 * of the last byte to byte mode, even if word mode is used */
                case QSPIIOCS_ODD_MOD:
                        dev->odd_mod = (arg ? 1 : 0);
                        break;

                /* Set data buffer to be used as "send data" during reads */
                case QSPIIOCS_READDATA:
                        read_data = (struct qspi_read_data *)arg;
                        error = verify_area(VERIFY_READ, read_data,
                                        sizeof(struct qspi_read_data));
                        if (error) {
                                ret = error;
                                break;
                        }

                        if (read_data->length > sizeof(read_data->buf)) {
                                ret = -EINVAL;
                                break;
                        }

                        copy_from_user(&dev->read_data, read_data,
                                        sizeof(struct qspi_read_data));
                        break;

                /* Set data buffer to be used as "send data" during reads */
                case QSPIIOCS_READKDATA:
                        read_data = (struct qspi_read_data *)arg;
                        if (read_data->length > sizeof(read_data->buf)) {
                                ret = -EINVAL;
                                break;
                        }
                        memcpy (&dev->read_data, read_data,
                                        sizeof(struct qspi_read_data));
                        break;

                /* Set driver to use polling mode, which may increase
                 * performance for small transfers */
                case QSPIIOCS_POLL_MOD:
                        dev->poll_mod = (arg ? 1 : 0);
                        break;

                default:
                        ret = -EINVAL;
                        break;
        }

        up(&sem);
        return(ret);
}

/**
*******************************************************************************
* \brief        coldfire qspi device open function
*
******************************************************************************/
void *spi_dev_open(int num)
{
        qspi_dev *dev;

        if ((dev = kmalloc(sizeof(qspi_dev), GFP_KERNEL)) == NULL) {
                return(NULL);
        }

        /* set default values */
        dev->read_data.length = 0;
        dev->read_data.buf[0] = 0;
        dev->read_data.buf[1] = 0;
        dev->read_data.loop = 0;
        dev->poll_mod = 0;              /* interrupt mode */
        dev->bits = 8;
        dev->cpol = 0;
        dev->cpha = 0;
        dev->qcr_cont = 1;
        dev->dsp_mod = 0;               /* no DSP mode */
        dev->odd_mod = 0;               /* no ODD mode */
        dev->qcd = 17;
        dev->dtl = 1;
        dev->num = num;			/* device number */

        return(dev);
}

/**
*******************************************************************************
* \brief        coldfire qspi release function
*
******************************************************************************/
int spi_dev_release(void *dev)
{
        kfree(dev);

        return(0);
}

/**
*******************************************************************************
* \brief        coldfire qspi read function
*
******************************************************************************/
ssize_t spi_dev_read(void *device, char *buffer, size_t length, int memtype)
{
        qspi_dev *dev = (qspi_dev *)device;
        int qcr_cs;
        int total = 0;
        int i = 0;
        int z;
        int max_trans;
        unsigned char bits;
        unsigned char word = 0;
        unsigned long flag;
        int rdi = 0;
        int is_last_odd = 0;

        down(&sem);

        /* set the register with default values */
        QMR = QMR_MSTR |
                (dev->dohie << 14) |
                (dev->bits << 10) |
                (dev->cpol << 9) |
                (dev->cpha << 8) |
                (dev->baud);

        QDLYR = (dev->qcd << 8) | dev->dtl;

        if (dev->dsp_mod)
                max_trans = 15;
        else
                max_trans = 16;

        if (dev->odd_mod)
        	z = QCR_SETUP8;
        else
        	z = QCR_SETUP;


        qcr_cs = (~dev->num << 8) & 0xf00;  /* CS for QCR */
//      qcr_cs = 0;  // temporarilly force CS0 only, until can check minor device number processing!!

        bits = dev->bits % 0x10;
        if (bits == 0 || bits > 0x08)
                word = 1; /* 9 to 16bit transfers */

//              printk("\n READ driver -- ioctl xmit data fm dev->read_data.buf array  %x %x %x %x \n",dev->read_data.buf[0],dev->read_data.buf[1],dev->read_data.buf[2],dev->read_data.buf[3]);

        while (i < length) {
                unsigned short *sp = (unsigned short *)&buffer[i];
                unsigned char *cp = &buffer[i];
                unsigned short *rd_sp = (unsigned short *)dev->read_data.buf;
                int x;
                int n;

                QAR = TX_RAM_START;             /* address first QTR */
                n = 0;
                while(n < max_trans) {
                        if (rdi != -1) {
                                if (word) {
                                        QDR = rd_sp[rdi++];
                                        if (rdi == dev->read_data.length >> 1)
                                                rdi = dev->read_data.loop ? 0 : -1;
                                } else {
                                        QDR = dev->read_data.buf[rdi++];
                                        if (rdi == dev->read_data.length)
                                                rdi = dev->read_data.loop ? 0 : -1;
                                }
                        } else
                                QDR = 0;

                        i++;
                        n++;

                        if (word) {
                            if (dev->odd_mod && (i == length)){
                                is_last_odd = 1;
                                break;
                            }
                            i++;
                        }

                        if (i >= length)
                                break;
                }

                QAR = COMMAND_RAM_START;        /* address first QCR */
                for (x = 0; x < n; x++) {
                        /* QCR write */
              		if (dev->qcr_cont) {
                       		if (x == n - 1 && i == length) {
                           		if ((i % 2) != 0) {
                               			QDR = z | qcr_cs; /* last transfer and odd number of chars */
                           		} else {
                               			QDR = QCR_SETUP | qcr_cs; /* last transfer */
                           		}
                       		} else {
                           		QDR = QCR_CONT | QCR_SETUP | qcr_cs;
                       		}
                    	} else {
                       		if (x == n - 1 && i == length)
                           		QDR = z | qcr_cs; /* last transfer */
                       		else
                           		QDR = QCR_SETUP | qcr_cs;
                   	}
  		}
                QWR = QWR_CSIV | ((n - 1) << 8);

                /* check if we are using polling mode. Polling increases
                 * performance for samll data transfers but is dangerous
                 * if we stay too long here, locking other tasks!!
                 */
                if (dev->poll_mod) {
                        QIR = QIR_SETUP_POLL;
                        QDLYR |= QDLYR_SPE;

                        while ((QIR & QIR_SPIF) != QIR_SPIF)
                                ;
                        QIR = QIR | QIR_SPIF;
                } else {
                        QIR = QIR_SETUP;
                        save_flags(flag); cli();                // like in write function

                        QDLYR |= QDLYR_SPE;
//                      interruptible_sleep_on(&wqueue);
                        sleep_on(&wqueue);                      // changed richard@opentcp.org
                        restore_flags(flag);                    // like in write function

                }

                QAR = RX_RAM_START;     /* address: first QRR */
		if (word) {
      			/* 9 to 16bit transfers */
                        if (is_last_odd) { //odd mode and odd number of chars
                        	for (x = 0; x < n; x++) {
                                	if (memtype == MEM_TYPE_USER){
                                                if (x == n - 1) { //last transfer
                                                        put_user(*(volatile unsigned short *)(MCF_MBAR + MCFSIM_QDR), cp++);
                                                } else {
                                                        put_user(*(volatile unsigned short *)(MCF_MBAR + MCFSIM_QDR), sp++);
                                                        cp +=2;
                                                }
                                        } else {
                                                if (x == n - 1) { //last transfer
                                                        *cp++ = *(volatile unsigned short *)(MCF_MBAR + MCFSIM_QDR);
                                                } else {
                                                        *sp++ = *(volatile unsigned short *)(MCF_MBAR + MCFSIM_QDR);
                                                        cp +=2;
                                                }
                                        }
                        	}
                    	} else {
                        	if (memtype == MEM_TYPE_USER){
                                        for (x = 0; x < n; x++) {
                                                put_user(*(volatile unsigned short *)(MCF_MBAR + MCFSIM_QDR), sp++);
                                                cp +=2;
                                        }
                                } else {
                                        for (x = 0; x < n; x++) {
                                                *sp++ = *(volatile unsigned short *)(MCF_MBAR + MCFSIM_QDR);
                                                cp +=2;
                                        }
                                }
                     	}
                } else {
                        /* 8bit transfers */
                       	if (memtype == MEM_TYPE_USER){
                                for (x = 0; x < n; x++)
                                        put_user(*(volatile unsigned short *)(MCF_MBAR + MCFSIM_QDR), cp++);
                        } else {
                                for (x = 0; x < n; x++)
                                        *cp++ = *(volatile unsigned short *)(MCF_MBAR + MCFSIM_QDR);
                        }
                }

                if (word) {
                	if (is_last_odd)
                    		n = (n << 1) - 1;
                        else
                        	n <<= 1;
                }

                total += n;
        }

        up(&sem);
        return(total);
}


/**
*******************************************************************************
* \brief        coldfire qspi write function
*
******************************************************************************/
ssize_t spi_dev_write(void *device, const char *buffer, size_t length,
                      int memtype)
{
        qspi_dev *dev = (qspi_dev *)device;
        int qcr_cs;
        int i = 0;
        int total = 0;
        int z;
        int max_trans;
        unsigned char bits;
        unsigned char word = 0;
        unsigned long flag;

        down(&sem);

        QMR = QMR_MSTR |
                (dev->dohie << 14) |
                (dev->bits << 10) |
                (dev->cpol << 9) |
                (dev->cpha << 8) |
                (dev->baud);

        QDLYR = (dev->qcd << 8) | dev->dtl;

        bits = (QMR >> 10) % 0x10;
        if (bits == 0 || bits > 0x08)
                word = 1;       /* 9 to 16 bit transfers */

        qcr_cs = (~dev->num << 8) & 0xf00;       /* CS for QCR */
                                  /* next line was memcpy_fromfs()  */
//      qcr_cs = 0; // for testing, to force CS0 always
        if (memtype == MEM_TYPE_USER){
                copy_from_user (dbuf, buffer, length);
        } else {
		memcpy (dbuf, buffer, length);
        }

//      printk("data to write is %x  %x  %x  %X  \n",dbuf[0],dbuf[1],dbuf[2],dbuf[3]);

        if (dev->odd_mod)
                z = QCR_SETUP8;
        else
                z = QCR_SETUP;

        if (dev->dsp_mod)
                max_trans = 15;
        else
                max_trans = 16;

        while (i < length) {
                int x;
                int n;

                QAR = TX_RAM_START;             /* address: first QTR */
                if (word) {
                        for (n = 0; n < max_trans; ) {
                                /* in odd mode last byte will be transfered in byte mode */
                                if (dev->odd_mod && (i + 1 == length)) {
                                        QDR = dbuf[i];  /* tx data: QDR write */
                                        // printk("0x%X ", dbuf[i]);
                                        n++;
                                        i++;
                                        break;
                                }
                                else {
                                        QDR = (dbuf[i] << 8) + dbuf[i+1]; /* tx data: QDR write */
                                        //printk("0x%X 0x%X ", dbuf[i], dbuf[i+1]);
                                        n++;
                                        i += 2;
                                        if (i >= length)
                                                break;
                                }
                        }
                } else {
                        /* 8bit transfers */
                        for (n = 0; n < max_trans; ) {
                                QDR = dbuf[i];  /* tx data: QTR write */
                                n++;
                                i++;
                                if (i == length)
                                        break;
                        }
                }

                QAR = COMMAND_RAM_START;        /* address: first QCR */
                for (x = 0; x < n; x++) {
                        /* QCR write */
                        if (dev->qcr_cont) {
                                if (x == n-1 && i == length)
                                        if ((i % 2)!= 0)
                                                QDR = z | qcr_cs; /* last transfer and odd number of chars */
                                        else
                                                QDR = QCR_SETUP | qcr_cs;       /* last transfer */
                                else
                                        QDR = QCR_CONT | QCR_SETUP | qcr_cs;
                        } else {
                                if (x == n - 1 && i == length)
                                        QDR = z | qcr_cs; /* last transfer */
                                else
                                        QDR = QCR_SETUP | qcr_cs;
                        }
                }

                QWR = QWR_CSIV | ((n - 1) << 8);  /* QWR[ENDQP] = n << 8 */

                /* check if we are using polling mode. Polling increases
                 * performance for samll data transfers but is dangerous
                 * if we stay too long here, locking other tasks!!
                 */
                if (dev->poll_mod) {
                        QIR = QIR_SETUP_POLL;
                        QDLYR |= QDLYR_SPE;

                        while ((QIR & QIR_SPIF) != QIR_SPIF)
                                ;
                        QIR = QIR | QIR_SPIF;
                } else {
                        QIR = QIR_SETUP;
                        save_flags(flag); cli();                // added according to gerg@snapgear.com
                        QDLYR |= QDLYR_SPE;

//                      interruptible_sleep_on(&wqueue);
                        sleep_on(&wqueue);                      // changed richard@opentcp.org

                        restore_flags(flag);                    // added according to gerg@snapgear.com
                }


                if (word)
                        n <<= 1;

                total += n;
        }

        up(&sem);
        return(total);
}

/**
*******************************************************************************
* \brief        coldfire qspi device init function
*
******************************************************************************/
int __init spi_dev_init(void)
{
        volatile u32 *lp;
#if defined(CONFIG_M5249) || defined(CONFIG_M5282) || defined(CONFIG_M5280)
        volatile u8 *cp;
#endif

        if (request_irq(MCFQSPI_IRQ_VECTOR, spi_dev_interrupt, SA_INTERRUPT, "ColdFire QSPI", NULL)) {
                printk("QSPI: Unable to attach ColdFire QSPI interrupt "
                        "vector=%d\n", MCFQSPI_IRQ_VECTOR);
                return(-EINVAL);
        }

#if defined(CONFIG_M5249)
        cp = (volatile u8 *)(MCF_MBAR + MCFSIM_ICR10);
        *cp = 0x8f;             /* autovector on, il=3, ip=3 */

        lp = (volatile u32 *)(MCF_MBAR2 + 0x180);
        *lp |= 0x00000800;      /* activate qspi_in and qspi_clk */

        lp = (volatile u32 *)(MCF_MBAR2 + MCFSIM2_GPIOFUNC);
        *lp &= 0xdc9FFFFF;      /* activate qspi_cs0 .. 3, qspi_dout */

        lp = (volatile u32 *)(MCF_MBAR + MCFSIM_IMR);
        *lp &= 0xFFFbFFFF;      /* enable qspi interrupt */
#elif defined(CONFIG_M5282) || defined(CONFIG_M5280)
	cp = (volatile u8 *) (MCF_IPSBAR + MCFICM_INTC0 + MCFINTC_ICR0 +
			MCFINT_QSPI);
	*cp = (5 << 3) + 3;     /* level 5, priority 3 */

	cp = (volatile u8 *) (MCF_IPSBAR + MCF5282_GPIO_PQSPAR);
	*cp = 0x7f;             /* activate din, dout, clk and cs[0..3] */

	lp = (volatile u32 *) (MCF_IPSBAR + MCFICM_INTC0 + MCFINTC_IMRL);
	*lp &= ~(1 + (1 << MCFINT_QSPI));      /* enable qspi interrupt */
#else
        /* set our IPL */
        lp = (volatile u32 *)(MCF_MBAR + MCFSIM_ICR4);
        *lp = (*lp & 0x07777777) | 0xd0000000;

        /* 1) CS pin setup 17.2.x
         *      Dout, clk, cs0 always enabled. Din, cs[3:1] must be enabled.
         *      CS1: PACNT[23:22] = 10
         *      CS1: PBCNT[23:22] = 10 ?
         *      CS2: PDCNT[05:04] = 11
         *      CS3: PACNT[15:14] = 01
         */
        lp = (volatile u32 *)(MCF_MBAR + MCFSIM_PACNT);
        *lp = (*lp & 0xFF3F3FFF) | 0x00804000;  /* 17.2.1 QSPI CS1 & CS3 */
        lp = (volatile u32 *)(MCF_MBAR + MCFSIM_PDCNT);
        *lp = (*lp & 0xFFFFFFCF) | 0x00000030;  /* QSPI_CS2 */
#endif

        /*
         * These values have to be setup according to the applications
         * using the qspi driver. Maybe some #defines at the beginning
         * would be more appropriate. Especially the transfer size
         * and speed settings
         */
        QMR = 0xA1A2; // default mode setup: 8 bits, baud, 160kHz clk.
//      QMR = 0x81A2; // default mode setup: 16 bits, baud, 160kHz clk.
        QDLYR = 0x0202; // default start & end delays

        init_waitqueue_head(&wqueue);    /* was init_waitqueue() --Ron */

#if defined(CONFIG_M5249)
        printk("MCF5249 QSPI driver %s\n", MCF_QSPI_VERSION);
#elif defined(CONFIG_M5282) || defined(CONFIG_M5280)
	printk("MCF5282 QSPI driver ok\n");
#else
        printk("MCF5272 QSPI driver ok %s\n", MCF_QSPI_VERSION);
#endif

        return(0);
}


/* Cleanup - undid whatever init_module did */
void __exit spi_dev_exit(void)      /* the __exit added by ron  */
{

        free_irq(MCFQSPI_IRQ_VECTOR, NULL);

#if defined(CONFIG_M5249)
        /* autovector on, il=0, ip=0 */
        *(volatile u8 *)(MCF_MBAR + MCFSIM_ICR10) = 0x80;
        /* disable qspi interrupt */
        *(volatile u32 *)(MCF_MBAR + MCFSIM_IMR) |= 0x00040000;
#elif defined(CONFIG_M5282)
	/* interrupt level 0, priority 0 */
	*(volatile u8 *) (MCF_IPSBAR + MCFICM_INTC0 +
			MCFINTC_ICR0 + MCFINT_QSPI) = 0;
	/* disable qspi interrupt */
	*(volatile u32 *) (MCF_IPSBAR + MCFICM_INTC0 + MCFINTC_IMRL)
		|= (1 << MCFINT_QSPI);
#else
        /* zero our IPL */
        *((volatile u32 *)(MCF_MBAR + MCFSIM_ICR4)) = 0x80000000;
#endif

}

module_init(spi_dev_init);
module_exit(spi_dev_exit);

EXPORT_SYMBOL(spi_dev_read);
EXPORT_SYMBOL(spi_dev_write);
EXPORT_SYMBOL(spi_dev_open);
EXPORT_SYMBOL(spi_dev_release);
EXPORT_SYMBOL(spi_dev_ioctl);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Josef Baumgartner <josef.baumgartner@telex.de>");
MODULE_DESCRIPTION("SPI-Bus motorola coldfire qspi driver module");


