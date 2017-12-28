/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2004-2008 Cavium Networks
 */
#include "hal.h"
#include <asm/mach-cavium-octeon/gpio.h>

unsigned long octeon_gpio_read(void)
{
	cvmx_gpio_rx_dat_t gpio_rx_dat;
	gpio_rx_dat.u64 = cvmx_read_csr(CVMX_GPIO_RX_DAT);
	return gpio_rx_dat.s.dat;

}

void octeon_gpio_clear(unsigned long bits)
{
	cvmx_gpio_tx_clr_t gpio_tx_clr;
	gpio_tx_clr.u64 = 0;
	gpio_tx_clr.s.clr = bits;
	cvmx_write_csr(CVMX_GPIO_TX_CLR, gpio_tx_clr.u64);
}

void octeon_gpio_set(unsigned long bits)
{
	cvmx_gpio_tx_set_t gpio_tx_set;
	gpio_tx_set.u64 = 0;
	gpio_tx_set.s.set = bits;
	cvmx_write_csr(CVMX_GPIO_TX_SET, gpio_tx_set.u64);
}

void octeon_gpio_config(int line, int type)
{
	cvmx_gpio_xbit_cfgx_t gpio_cfgx;
	gpio_cfgx.u64 = type;
	cvmx_write_csr(CVMX_GPIO_BIT_CFGX(line), gpio_cfgx.u64);
}

void octeon_gpio_interrupt_ack(int line)
{
	cvmx_gpio_int_clr_t gpio_int_clr;
	gpio_int_clr.u64 = 0;
	gpio_int_clr.s.type = 0x1 << line;
	cvmx_write_csr(CVMX_GPIO_INT_CLR, gpio_int_clr.u64);
}

