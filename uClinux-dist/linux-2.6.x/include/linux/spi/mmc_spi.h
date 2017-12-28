#ifndef __LINUX_SPI_MMC_SPI_H
#define __LINUX_SPI_MMC_SPI_H

#include <linux/mmc/protocol.h>
#include <linux/interrupt.h>

struct device;
struct mmc_host;

/* something to put in platform_data of a device being used
 * to manage an MMC/SD card slot
 *
 * REVISIT this isn't spi-specific, any card slot should be
 * able to handle it
 */
struct mmc_spi_platform_data {
	/* driver activation and (optional) card detect irq */
	int (*init)(struct device *,
		irqreturn_t (*)(int, void *),
		void *);
	void (*exit)(struct device *, void *);

	/* how long to debounce card detect, in jiffies */
	unsigned long detect_delay;

	/* sense switch on sd cards */
	int (*get_ro)(struct device *);

	/* power management */
	unsigned int ocr_mask;			/* available voltages */
	void (*setpower)(struct device *, unsigned int);
};

#endif /* __LINUX_SPI_MMC_SPI_H */
