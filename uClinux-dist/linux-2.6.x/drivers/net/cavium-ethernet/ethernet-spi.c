/*************************************************************************
* Cavium Octeon Ethernet Driver
*
* Author: Cavium Networks info@caviumnetworks.com
*
* Copyright (c) 2003-2007  Cavium Networks (support@cavium.com). All rights
* reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
*
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*
*     * Redistributions in binary form must reproduce the above
*       copyright notice, this list of conditions and the following
*       disclaimer in the documentation and/or other materials provided
*       with the distribution.
*
*     * Neither the name of Cavium Networks nor the names of
*       its contributors may be used to endorse or promote products
*       derived from this software without specific prior written
*       permission.
*
* This Software, including technical data, may be subject to U.S.  export
* control laws, including the U.S.  Export Administration Act and its
* associated regulations, and may be subject to export or import regulations
* in other countries.  You warrant that You will comply strictly in all
* respects with all such regulations and acknowledge that you have the
* responsibility to obtain licenses to export, re-export or import the
* Software.
*
* TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
* AND WITH ALL FAULTS AND CAVIUM NETWORKS MAKES NO PROMISES, REPRESENTATIONS
* OR WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH
* RESPECT TO THE SOFTWARE, INCLUDING ITS CONDITION, ITS CONFORMITY TO ANY
* REPRESENTATION OR DESCRIPTION, OR THE EXISTENCE OF ANY LATENT OR PATENT
* DEFECTS, AND CAVIUM SPECIFICALLY DISCLAIMS ALL IMPLIED (IF ANY) WARRANTIES
* OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A PARTICULAR
* PURPOSE, LACK OF VIRUSES, ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET
* POSSESSION OR CORRESPONDENCE TO DESCRIPTION.  THE ENTIRE RISK ARISING OUT
* OF USE OR PERFORMANCE OF THE SOFTWARE LIES WITH YOU.
*************************************************************************/
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/mii.h>
#include <net/dst.h>

#include "wrapper-cvmx-includes.h"
#include "ethernet-headers.h"

static int number_spi_ports = 0;
static int need_retrain[2] = {0,0};

static irqreturn_t cvm_oct_spi_rml_interrupt(int cpl, void *dev_id)
{
    irqreturn_t return_status = IRQ_NONE;
    cvmx_npi_rsl_int_blocks_t rsl_int_blocks;

    /* Check and see if this interrupt was caused by the GMX block */
    rsl_int_blocks.u64 = cvmx_read_csr(CVMX_NPI_RSL_INT_BLOCKS);
    if (rsl_int_blocks.s.spx1) /* 19 - SPX1_INT_REG & STX1_INT_REG */
    {
        cvmx_spxx_int_reg_t spx_int_reg;
        cvmx_stxx_int_reg_t stx_int_reg;

        spx_int_reg.u64 = cvmx_read_csr(CVMX_SPXX_INT_REG(1));
        cvmx_write_csr(CVMX_SPXX_INT_REG(1), spx_int_reg.u64);
        if (!need_retrain[1])
        {
            spx_int_reg.u64 &= cvmx_read_csr(CVMX_SPXX_INT_MSK(1));
            if (spx_int_reg.s.spf)      printk("SPI1: SRX Spi4 interface down\n");
            if (spx_int_reg.s.calerr)   printk("SPI1: SRX Spi4 Calendar table parity error\n");
            if (spx_int_reg.s.syncerr)  printk("SPI1: SRX Consecutive Spi4 DIP4 errors have exceeded SPX_ERR_CTL[ERRCNT]\n");
            if (spx_int_reg.s.diperr)   printk("SPI1: SRX Spi4 DIP4 error\n");
            if (spx_int_reg.s.tpaovr)   printk("SPI1: SRX Selected port has hit TPA overflow\n");
            if (spx_int_reg.s.rsverr)   printk("SPI1: SRX Spi4 reserved control word detected\n");
            if (spx_int_reg.s.drwnng)   printk("SPI1: SRX Spi4 receive FIFO drowning/overflow\n");
            if (spx_int_reg.s.clserr)   printk("SPI1: SRX Spi4 packet closed on non-16B alignment without EOP\n");
            if (spx_int_reg.s.spiovr)   printk("SPI1: SRX Spi4 async FIFO overflow\n");
            if (spx_int_reg.s.abnorm)   printk("SPI1: SRX Abnormal packet termination (ERR bit)\n");
            if (spx_int_reg.s.prtnxa)   printk("SPI1: SRX Port out of range\n");
        }

        stx_int_reg.u64 = cvmx_read_csr(CVMX_STXX_INT_REG(1));
        cvmx_write_csr(CVMX_STXX_INT_REG(1), spx_int_reg.u64);
        if (!need_retrain[1])
        {
            stx_int_reg.u64 &= cvmx_read_csr(CVMX_STXX_INT_MSK(1));
            if (stx_int_reg.s.syncerr)  printk("SPI1: STX Interface encountered a fatal error\n");
            if (stx_int_reg.s.frmerr)   printk("SPI1: STX FRMCNT has exceeded STX_DIP_CNT[MAXFRM]\n");
            if (stx_int_reg.s.unxfrm)   printk("SPI1: STX Unexpected framing sequence\n");
            if (stx_int_reg.s.nosync)   printk("SPI1: STX ERRCNT has exceeded STX_DIP_CNT[MAXDIP]\n");
            if (stx_int_reg.s.diperr)   printk("SPI1: STX DIP2 error on the Spi4 Status channel\n");
            if (stx_int_reg.s.datovr)   printk("SPI1: STX Spi4 FIFO overflow error\n");
            if (stx_int_reg.s.ovrbst)   printk("SPI1: STX Transmit packet burst too big\n");
            if (stx_int_reg.s.calpar1)  printk("SPI1: STX Calendar Table Parity Error Bank1\n");
            if (stx_int_reg.s.calpar0)  printk("SPI1: STX Calendar Table Parity Error Bank0\n");
        }

        cvmx_write_csr(CVMX_SPXX_INT_MSK(1), 0);
        cvmx_write_csr(CVMX_STXX_INT_MSK(1), 0);
        need_retrain[1] = 1;
        return_status = IRQ_HANDLED;
    }

    if (rsl_int_blocks.s.spx0) /* 18 - SPX0_INT_REG & STX0_INT_REG */
    {
        cvmx_spxx_int_reg_t spx_int_reg;
        cvmx_stxx_int_reg_t stx_int_reg;

        spx_int_reg.u64 = cvmx_read_csr(CVMX_SPXX_INT_REG(0));
        cvmx_write_csr(CVMX_SPXX_INT_REG(0), spx_int_reg.u64);
        if (!need_retrain[0])
        {
            spx_int_reg.u64 &= cvmx_read_csr(CVMX_SPXX_INT_MSK(0));
            if (spx_int_reg.s.spf)      printk("SPI0: SRX Spi4 interface down\n");
            if (spx_int_reg.s.calerr)   printk("SPI0: SRX Spi4 Calendar table parity error\n");
            if (spx_int_reg.s.syncerr)  printk("SPI0: SRX Consecutive Spi4 DIP4 errors have exceeded SPX_ERR_CTL[ERRCNT]\n");
            if (spx_int_reg.s.diperr)   printk("SPI0: SRX Spi4 DIP4 error\n");
            if (spx_int_reg.s.tpaovr)   printk("SPI0: SRX Selected port has hit TPA overflow\n");
            if (spx_int_reg.s.rsverr)   printk("SPI0: SRX Spi4 reserved control word detected\n");
            if (spx_int_reg.s.drwnng)   printk("SPI0: SRX Spi4 receive FIFO drowning/overflow\n");
            if (spx_int_reg.s.clserr)   printk("SPI0: SRX Spi4 packet closed on non-16B alignment without EOP\n");
            if (spx_int_reg.s.spiovr)   printk("SPI0: SRX Spi4 async FIFO overflow\n");
            if (spx_int_reg.s.abnorm)   printk("SPI0: SRX Abnormal packet termination (ERR bit)\n");
            if (spx_int_reg.s.prtnxa)   printk("SPI0: SRX Port out of range\n");
        }

        stx_int_reg.u64 = cvmx_read_csr(CVMX_STXX_INT_REG(0));
        cvmx_write_csr(CVMX_STXX_INT_REG(0), spx_int_reg.u64);
        if (!need_retrain[0])
        {
            stx_int_reg.u64 &= cvmx_read_csr(CVMX_STXX_INT_MSK(0));
            if (stx_int_reg.s.syncerr)  printk("SPI0: STX Interface encountered a fatal error\n");
            if (stx_int_reg.s.frmerr)   printk("SPI0: STX FRMCNT has exceeded STX_DIP_CNT[MAXFRM]\n");
            if (stx_int_reg.s.unxfrm)   printk("SPI0: STX Unexpected framing sequence\n");
            if (stx_int_reg.s.nosync)   printk("SPI0: STX ERRCNT has exceeded STX_DIP_CNT[MAXDIP]\n");
            if (stx_int_reg.s.diperr)   printk("SPI0: STX DIP2 error on the Spi4 Status channel\n");
            if (stx_int_reg.s.datovr)   printk("SPI0: STX Spi4 FIFO overflow error\n");
            if (stx_int_reg.s.ovrbst)   printk("SPI0: STX Transmit packet burst too big\n");
            if (stx_int_reg.s.calpar1)  printk("SPI0: STX Calendar Table Parity Error Bank1\n");
            if (stx_int_reg.s.calpar0)  printk("SPI0: STX Calendar Table Parity Error Bank0\n");
        }

        cvmx_write_csr(CVMX_SPXX_INT_MSK(0), 0);
        cvmx_write_csr(CVMX_STXX_INT_MSK(0), 0);
        need_retrain[0] = 1;
        return_status = IRQ_HANDLED;
    }

    return return_status;
}

static void cvm_oct_spi_enable_error_reporting(int interface)
{
    cvmx_spxx_int_msk_t spxx_int_msk;
    cvmx_stxx_int_msk_t stxx_int_msk;

    spxx_int_msk.u64 = cvmx_read_csr(CVMX_SPXX_INT_MSK(interface));
    spxx_int_msk.s.calerr = 1;
    spxx_int_msk.s.syncerr = 1;
    spxx_int_msk.s.diperr = 1;
    spxx_int_msk.s.tpaovr = 1;
    spxx_int_msk.s.rsverr = 1;
    spxx_int_msk.s.drwnng = 1;
    spxx_int_msk.s.clserr = 1;
    spxx_int_msk.s.spiovr = 1;
    spxx_int_msk.s.abnorm = 1;
    spxx_int_msk.s.prtnxa = 1;
    cvmx_write_csr(CVMX_SPXX_INT_MSK(interface), spxx_int_msk.u64);

    stxx_int_msk.u64 = cvmx_read_csr(CVMX_STXX_INT_MSK(interface));
    stxx_int_msk.s.frmerr = 1;
    stxx_int_msk.s.unxfrm = 1;
    stxx_int_msk.s.nosync = 1;
    stxx_int_msk.s.diperr = 1;
    stxx_int_msk.s.datovr = 1;
    stxx_int_msk.s.ovrbst = 1;
    stxx_int_msk.s.calpar1 = 1;
    stxx_int_msk.s.calpar0 = 1;
    cvmx_write_csr(CVMX_STXX_INT_MSK(interface), stxx_int_msk.u64);
}

static void cvm_oct_spi_poll(struct net_device *dev)
{
    static int spi4000_port = 0;
    cvm_oct_private_t* priv = (cvm_oct_private_t*)netdev_priv(dev);
    int interface;

    for (interface=0; interface<2; interface++)
    {
        if ((priv->port == interface*16) && need_retrain[interface])
        {
            if (cvmx_spi_restart_interface(interface, CVMX_SPI_MODE_DUPLEX, 10) == 0)
            {
                need_retrain[interface] = 0;
                cvm_oct_spi_enable_error_reporting(interface);
            }
        }
        /* The SPI4000 TWSI interface is very slow. In order not to bring the
            system to a crawl, we only poll a single port every second. This
            means negotiation speed changes take up to 10 seconds, but at least
            we don't waste absurd amounts of time waiting for TWSI */
        if (priv->port == spi4000_port)
        {
            /* This function does nothing if it is called on an interface
                without a SPI4000 */
            cvmx_spi4000_check_speed(interface, priv->port);
            /* Normal ordering increments. By decrimenting we only match once
                per iteration */
            spi4000_port--;
            if (spi4000_port < 0)
                spi4000_port = 10;
        }
    }
}


int cvm_oct_spi_init(struct net_device *dev)
{
    cvm_oct_private_t* priv = (cvm_oct_private_t*)netdev_priv(dev);

    if (number_spi_ports == 0)
    {
        if (request_irq(8 + 46, cvm_oct_spi_rml_interrupt, IRQF_SHARED, "SPI", &number_spi_ports))
	    printk("cavium-ethernet: failed to register irq=%d\n", 8 + 46);
    }
    number_spi_ports++;

    if ((priv->port == 0) || (priv->port == 16))
    {
        cvm_oct_spi_enable_error_reporting(INTERFACE(priv->port));
        priv->poll = cvm_oct_spi_poll;
    }
    cvm_oct_common_init(dev);
    return 0;
}

void cvm_oct_spi_uninit(struct net_device *dev)
{
    int interface;

    cvm_oct_common_uninit(dev);
    number_spi_ports--;
    if (number_spi_ports == 0)
    {
        for (interface=0; interface<2; interface++)
        {
            cvmx_write_csr(CVMX_SPXX_INT_MSK(interface), 0);
            cvmx_write_csr(CVMX_STXX_INT_MSK(interface), 0);
        }
        free_irq(8 + 46, &number_spi_ports);
    }
}

