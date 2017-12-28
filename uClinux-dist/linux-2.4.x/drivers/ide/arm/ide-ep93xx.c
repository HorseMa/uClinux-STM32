/*****************************************************************************
 *
 * drivers/ide/ide-ep93xx.c
 *
 *  Copyright (c) 2003 Cirrus Logic, Inc.
 *
 * Some code based in whole or in part on:
 *
 *      linux/drivers/ide/ide-dma.c		Version 4.13	May 21, 2003
 *
 *      Copyright (c) 1999-2000	Andre Hedrick <andre@linux-ide.org>
 *      May be copied or modified under the terms of the GNU General Public License
 *
 *      Portions Copyright Red Hat 2003
 *
 *      Special Thanks to Mark for his Six years of work.
 *
 *      Copyright (c) 1995-1998  Mark Lord
 *      May be copied or modified under the terms of the GNU General Public License
 *
 *
 * Some code taken from the PowerMac implentation
 *
 *      Copyright (C) 1998-2001 Paul Mackerras & Ben. Herrenschmidt
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * ***************************************************************************
 *
 * Cirrus Logic's EP93XX architecture specific ide driver. This driver
 * supports the following ide transfer modes:
 *
 *     PIO mode 4
 *     UDMA modes 0 - 2
 *
 * This driver provides workarounds for the following hardware bugs:
 *
 * Write bug #1: If DDMARDYn is deasserted at the appropriate time (which is
 *               within the ATA spec. and is therefore valid), 32-bits of data
 *               to be written to the device is lost.  Since the upper 16-bits
 *               of that word is written to the IDE data bus, but never strobed
 *               to the device, this will also result in a CRC error (the
 *               EP93xx CRC generator will see the 16-bit value, but the device
 *               CRC generator will not).
 *
 *               This can be detected in two ways.  The first is the fact that
 *               the DMA controller will complete the transfer while the device
 *               is still requesting data.  This is not guaranteed to occur due
 *               to the second write bug; the data from one incident of this
 *               write bug will be offset by the data from two incidents of the
 *               second write bug (see below).  The other is to look for CRC
 *               errors.  If the device wants more data, the DMA controller is
 *               given more data 32-bits at a time until the device is happy.
 *
 * Write bug #2: If a data out burst is terminated after an odd number of
 *               16-bit words have been written to the device, the last 16-bit
 *               word of the burst is repeated as the first 16-bit word of the
 *               subsequent burst.  This also results in the last 16-bit word
 *               of data being "lost" since it is never written to the device.
 *               No CRC error occurs since the same data is seen by both the
 *               EP93xx CRC generator and the device CRC generator.  If this
 *               bug occurs only once during a single request, and the first
 *               write bug does not occur during that request, then this is an
 *               undetectable situation since the final "extra" 16-bit word is
 *               transferred to the device, which simply ignores it.
 *
 *               This can be detected by seeing that the device no longs wants
 *               data (it has deasserted DMARQ and has asserted INTRQ) but the
 *               DMA controller has more data to transfer.
 *
 *               A single instance of this bug in a request can be detected by
 *               reading back the data from the drive after each write.  This
 *               is obviously not acceptable from a performance point of view.
 *               Fortunately, this bug only appears to be triggered by old,
 *               small drives.
 *
 * Read bug #1:  If a data in burst is terminated after an odd number of
 *               16-bit words have been read from the device, the last 16-bit
 *               word of the burst is lost.  Like write bug #2, no CRC error
 *               occurs.  Note that this bug always occurs in pairs.  If it
 *               actually happens one time, there will always be an odd number
 *               of 16-bit words left to be read from the device.  Therefore,
 *               it will happen again either in the middle of the remaining
 *               data (leaving an even number of words remaining), or at the
 *               end of the request.
 *
 *               This can be detected by seeing that the device has no more
 *               data (it has deasserted DMARQ and has asserted INTRQ) but the
 *               DMA controller still expects more data to transfer.
 *
 * Once any of these bugs is detected, the request is retried.  If the request
 * can not be satisfied after RETRIES_PER_TRANSFER attempts, it is done in PIO
 * mode.  If FAILURES_BEFORE_BACKOFF requests in a row can not be completed
 * successfully (including retries), the transfer mode is backed down (i.e.
 * moving to UDMA1 instead of UDMA2).  Lower transfer modes are less likely
 * to be affected by these bugs, so a slower data transfer rate with less
 * retries will be better than a higher data transfer rate with more retries.
 *
 * The transfer mode and retries are managed independently for the two devices
 * on the IDE bus.  So, if the master device doesn't excite these bugs, but the
 * slave device does, the transfer rate of the master device isn't affected by
 * the slow down/retries on the slave device.
 *
 ****************************************************************************/
#include <linux/ide.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <asm/io.h>
#include <asm/ide.h>
#include <asm/irq.h>
#include <asm/arch/ide.h>
#include <asm/arch/dma.h>
#include <asm/hardware.h>

/*****************************************************************************
 *
 * Debugging macros
 *
 ****************************************************************************/
#undef DEBUG
//#define DEBUG 1
#ifdef DEBUG
#define DPRINTK( fmt, arg... )  printk( fmt, ##arg )
#else
#define DPRINTK( fmt, arg... )
#endif

/*****************************************************************************
 *
 * Functions and macros for handling dma transfers.
 *
 ****************************************************************************/
#define RETRIES_PER_TRANSFER			5
#define FAILURES_BEFORE_BACKOFF			50

/*****************************************************************************
 *
 * Global to keep track of the number of entries in the dma buffer
 * table for each transfer.
 *
 ****************************************************************************/
static int g_prd_count;
static unsigned int g_prd_total;
static unsigned int g_prd_returned;
static unsigned int *g_dma_table_base;

/*****************************************************************************
 *
 * Global to set during the DMA callback function to indicate that a DMA
 * error occurred (i.e. the DMA completed while the device was still
 * requesting).
 *
 ****************************************************************************/
static unsigned int g_bad = 0;

/*****************************************************************************
 *
 * A garbage word that is used to satify additional DMA requests from the
 * device.
 *
 ****************************************************************************/
static unsigned long g_garbage;

/*****************************************************************************
 *
 * The particulars of the most recent command that was executed.
 *
 ****************************************************************************/
static unsigned long g_cmd = 0;
static ide_drive_t *g_drive = NULL;
static unsigned long g_sector = 0;
static unsigned long g_nr_sectors = 0;
static unsigned long g_cur_retries = 0;

/*****************************************************************************
 *
 * The count of consective DMA request errors.  For each array, the first
 * entry is for hda reads, the second for hda writes, the third for hdb reads,
 * and the forth for hdb writes.
 *
 ****************************************************************************/
static unsigned long g_errors[4] = { 0, 0, 0, 0 };

/*****************************************************************************
 *
 * Some statistics that are gathered about the transfers.  For each array, the
 * first entry is for hda reads, the second for hda writes, the third for hdb
 * reads, and the fourth for hdb writes.
 *
 ****************************************************************************/
static unsigned long g_sectors[4] = { 0, 0, 0, 0 };
static unsigned long g_xfers[4] = { 0, 0, 0, 0 };
static unsigned long g_retries[4] = { 0, 0, 0, 0 };
static unsigned long g_max_retries[4] = { 0, 0, 0, 0 };

/*****************************************************************************
 *
 * Forward declarations.
 *
 ****************************************************************************/
static int ep93xx_ide_dma_on(ide_drive_t *drive);
static int ep93xx_ide_dma_off(ide_drive_t *drive);
static int ep93xx_ide_dma_begin(ide_drive_t *drive);
static int ep93xx_ide_dma_end(ide_drive_t *drive);
static int ep93xx_ide_dma_host_on(ide_drive_t *drive);

/*****************************************************************************
 *
 * The following is directly from ide-dma.c.
 *
 ****************************************************************************/
struct drive_list_entry
{
	const char * id_model;
	const char * id_firmware;
};

static const struct
drive_list_entry drive_blacklist [] =
{
	{ "WDC AC11000H",			"ALL"		},
	{ "WDC AC22100H",			"ALL"		},
	{ "WDC AC32500H",			"ALL"		},
	{ "WDC AC33100H",			"ALL"		},
	{ "WDC AC31600H",			"ALL"		},
	{ "WDC AC32100H",			"24.09P07"	},
	{ "WDC AC23200L",			"21.10N21"	},
	{ "Compaq CRD-8241B",			"ALL"		},
	{ "CRD-8400B",				"ALL"		},
	{ "CRD-8480B",				"ALL"		},
	{ "CRD-8480C",				"ALL"		},
	{ "CRD-8482B",				"ALL"		},
	{ "CRD-84",				"ALL"		},
	{ "SanDisk SDP3B",			"ALL"		},
	{ "SanDisk SDP3B-64",			"ALL"		},
	{ "SANYO CD-ROM CRD",			"ALL"		},
	{ "HITACHI CDR-8",			"ALL"		},
	{ "HITACHI CDR-8335",			"ALL"		},
	{ "HITACHI CDR-8435",			"ALL"		},
	{ "Toshiba CD-ROM XM-6202B",		"ALL"		},
	{ "CD-532E-A",				"ALL"		},
	{ "E-IDE CD-ROM CR-840",		"ALL"		},
	{ "CD-ROM Drive/F5A",			"ALL"		},
	{ "RICOH CD-R/RW MP7083A",		"ALL"		},
	{ "WPI CDD-820",			"ALL"		},
	{ "SAMSUNG CD-ROM SC-148C",		"ALL"		},
	{ "SAMSUNG CD-ROM SC-148F",		"ALL"		},
	{ "SAMSUNG CD-ROM SC",			"ALL"		},
	{ "SanDisk SDP3B-64",			"ALL"		},
	{ "SAMSUNG CD-ROM SN-124",		"ALL"		},
	{ "PLEXTOR CD-R PX-W8432T",		"ALL"		},
	{ "ATAPI CD-ROM DRIVE 40X MAXIMUM",	"ALL"		},
	{ "_NEC DV5800A",			"ALL"		},
	{ NULL,					NULL		}
};

/*****************************************************************************
 *
 *	This is directly from ide-dma.c
 *
 *  in_drive_list	-	look for drive in black/white list
 *	@id: drive identifier
 *	@drive_table: list to inspect
 *
 *	Look for a drive in the blacklist and the whitelist tables
 *	Returns 1 if the drive is found in the table.
 *
 ****************************************************************************/
static int
in_drive_list(struct hd_driveid *id,
              const struct drive_list_entry *drive_table)
{
	for ( ; drive_table->id_model ; drive_table++)
		if ((!strcmp(drive_table->id_model, id->model)) &&
		    ((!strstr(drive_table->id_firmware, id->fw_rev)) ||
		     (!strcmp(drive_table->id_firmware, "ALL"))))
			return 1;
	return 0;
}

/*****************************************************************************
 *
 * Return some statistics that are gathered about the operation of the IDE
 * driver.
 *
 ****************************************************************************/
#ifdef CONFIG_PROC_FS
static int
ep93xx_get_info(char *buffer, char **addr, off_t offset, int count)
{
	char *p = buffer;

	p += sprintf(p, "               sectors    requests     retries"
			                 "         max\n");
	p += sprintf(p, "hda read :  %10ld  %10ld  %10ld  %10ld\n",
		     g_sectors[0], g_xfers[0], g_retries[0], g_max_retries[0]);
	p += sprintf(p, "hda write:  %10ld  %10ld  %10ld  %10ld\n",
		     g_sectors[1], g_xfers[1], g_retries[1], g_max_retries[1]);
	p += sprintf(p, "hdb read :  %10ld  %10ld  %10ld  %10ld\n",
		     g_sectors[2], g_xfers[2], g_retries[2], g_max_retries[2]);
	p += sprintf(p, "hdb write:  %10ld  %10ld  %10ld  %10ld\n",
		     g_sectors[3], g_xfers[3], g_retries[3], g_max_retries[3]);

	return p - buffer;
}
#endif

/*****************************************************************************
 *
 * ep93xx_ide_dma_intr() is the handler for disk read/write DMA interrupts
 *
 ****************************************************************************/
static ide_startstop_t
ep93xx_ide_dma_intr (ide_drive_t *drive)
{
	struct request *rq = HWGROUP(drive)->rq;
	byte stat, dma_stat;
	int i;

	DPRINTK("%s: ep93xx_ide_dma_intr\n", drive->name);

	/*
	 * Disable the DMA for this transfer and cleanup.
	 */
	dma_stat = HWIF(drive)->ide_dma_end(drive);

	/*
	 * Get status from the ide device.
	 */
	stat = HWIF(drive)->INB(IDE_STATUS_REG);

	/*
	 * Retry the transfer if a DMA error occurred.
	 */
	if(dma_stat)
		return ide_stopped;

	/*
	 * See if the drive returned an error.
	 */
	if (OK_STAT(stat,DRIVE_READY,drive->bad_wstat|DRQ_STAT)) {
		/*
		 * Complete the request.
		 */
		for (i = rq->nr_sectors; i > 0;) {
			i -= rq->current_nr_sectors;
			DRIVER(drive)->end_request(drive, 1);
		}

		/*
		 * The transfer has been completed.
		 */
		return ide_stopped;
	}

	/*
	 * Print out an error.
	 */
	return DRIVER(drive)->error(drive, __FUNCTION__, stat);
}

/****************************************************************************
 *
 * Map a set of buffers described by scatterlist in streaming
 * mode for DMA.  This is the scather-gather version of the
 * above pci_map_single interface.  Here the scatter gather list
 * elements are each tagged with the appropriate dma address
 * and length.  They are obtained via sg_dma_{address,length}(SG).
 *
 * NOTE: An implementation may be able to use a smaller number of
 *       DMA address/length pairs than there are SG table elements.
 *       (for example via virtual mapping capabilities)
 *       The routine returns the number of addr/length pairs actually
 *       used, at most nents.
 *
 * Device ownership issues as mentioned above for pci_map_single are
 * the same here.
 *
 ****************************************************************************/
static inline int
ep93xx_map_sg(struct scatterlist *sg, unsigned int entries,
              unsigned int direction)
{
	unsigned int loop;

	for (loop = 0; loop < entries; loop++, sg++) {
		consistent_sync(sg->address, sg->length, direction);
		sg->dma_address = virt_to_bus(sg->address);
	}

	return entries;
}

/*****************************************************************************
 *
 * ide_build_sglist()
 *
 * Builds a table of buffers to be used by the dma.  Each buffer consists
 * of a region of memory which is contiguous in virtual memory.
 *
 ****************************************************************************/
static int
ide_build_sglist(ide_hwif_t *hwif, struct request *rq)
{
	struct buffer_head *buf_head;
	struct scatterlist *sg = hwif->sg_table;
	int nents = 0;

	DPRINTK("%s: ide_build_sglist: sg_table 0x%08lx\n", hwif->name,
		(unsigned long)hwif->sg_table);

	if (hwif->sg_dma_active)
		BUG();

	/*
	 * Set up the direction of the command
	 */
	if (rq->cmd == READ)
		hwif->sg_dma_direction = PCI_DMA_FROMDEVICE;
	else
		hwif->sg_dma_direction = PCI_DMA_TODEVICE;

	/*
	 * Get a pointer to the buffer head.
	 */
	buf_head = rq->bh;

	do {
		unsigned char *virt_addr = buf_head->b_data;
		unsigned int size = buf_head->b_size;

		if (nents >= PRD_ENTRIES)
			return 0;

		while ((buf_head = buf_head->b_reqnext) != NULL) {
			if ((virt_addr + size) !=
			    (unsigned char *)buf_head->b_data)
				break;

			size += buf_head->b_size;
		}
		memset(&sg[nents], 0, sizeof(*sg));
		sg[nents].address = virt_addr;
		sg[nents].length = size;
		nents++;
	} while (buf_head != NULL);

	/*
	 * This call to map_sg will return the number of entries
	 * for which DMAable memory could be mapped.
	 */
	return ep93xx_map_sg(sg, nents, hwif->sg_dma_direction);
}

/*****************************************************************************
 *
 * ide_build_dmatable() prepares a dma request.
 * Returns 0 if all went okay, returns 1 otherwise.
 *
 ****************************************************************************/
static int
ide_build_dmatable(ide_drive_t *drive)
{
	unsigned int *table = HWIF(drive)->dmatable_cpu = g_dma_table_base;
	unsigned int count = 0;
	int i;
	struct scatterlist *sg;

	DPRINTK("%s: ide_build_dmatable\n", drive->name);

	HWIF(drive)->sg_nents = i = ide_build_sglist(HWIF(drive),
						     HWGROUP(drive)->rq);

	if (!i)
		return 0;

	sg = HWIF(drive)->sg_table;

	while (i && sg_dma_len(sg)) {
		u32 cur_addr;
		u32 cur_len;

		cur_addr = sg_dma_address(sg);
		cur_len = sg_dma_len(sg);

		/*
		 * Fill in the dma table, without crossing any 64kB boundaries.
		 * Most hardware requires 16-bit alignment of all blocks,
		 * but the trm290 requires 32-bit alignment.
		 */
		while (cur_len) {
			if (count++ >= PRD_ENTRIES) {
				printk("%s: DMA table too small\n", drive->name);
				goto use_pio_instead;
			} else {
				u32 xcount, bcount = 0x10000 - (cur_addr & 0xffff);

				if (bcount > cur_len)
					bcount = cur_len;
				*table++ = cur_addr;
				xcount = bcount & 0xffff;
				if (xcount == 0x0000) {
					/*
					 * Most chipsets correctly interpret a
					 * length of 0x0000 as 64KB, but at
					 * least one (e.g. CS5530)
					 * misinterprets it as zero (!).  So
					 * here we break the 64KB entry into
					 * two 32KB entries instead.
					 */
					if (count++ >= PRD_ENTRIES) {
						printk("%s: DMA table too small\n",
						       drive->name);
						goto use_pio_instead;
					}
					*table++ = 0x8000;
					*table++ = cur_addr + 0x8000;
					xcount = 0x8000;
				}
				*table++ = xcount;
				cur_addr += bcount;
				cur_len -= bcount;
			}
		}
		sg++;
		i--;
	}

	if (count) {
		*--table |= 0x80000000;
		return count;
	}
	printk("%s: empty DMA table?\n", drive->name);

use_pio_instead:
	/*
	 * Revert to PIO for this request.
	 */
	HWIF(drive)->sg_dma_active = 0;
	return 0;
}

/*****************************************************************************
 *
 * ide_release_dma()
 *
 * This function releases the memory allocated for the scatter gather list
 * and for the dma table, and frees the dma channel.
 *
 ****************************************************************************/
static void
ep93xx_ide_release_dma(ide_hwif_t *hwif)
{
	DPRINTK("%s: ep93xx_ide_release_dma\n", hwif->name);

	/*
	 * Check if we have a valid dma handle.
	 */
	if (hwif->hw.dma != 0) {
		/*
		 * Pause the DMA channel.
		 */
		ep93xx_dma_pause(hwif->hw.dma, 1, 0);

		/*
		 * Flush the DMA channel
		 */
		ep93xx_dma_flush(hwif->hw.dma);
	}
}

/*****************************************************************************
 *
 * ep93xx_ide_callback()
 *
 * Registered with the ep93xx dma driver and called at the end of the dma
 * interrupt handler, this function should process the dma buffers.
 *
 ****************************************************************************/
static void
ep93xx_ide_callback(ep93xx_dma_int_t dma_int, ep93xx_dma_dev_t device,
                    unsigned int user_data)
{
	ide_drive_t *drive = (ide_drive_t *)user_data;
	ide_hwif_t *hwif = HWIF(drive);
	unsigned int temp;

	DPRINTK("ep93xx_ide_callback %d\n", dma_int);

	/*
	 * Retrieve from the dma interface as many used buffers as are
	 * available.
	 */
	while (ep93xx_dma_remove_buffer(hwif->hw.dma, &temp) == 0)
		g_prd_returned++;

	/*
	 * Add new buffers if we have any available.
	 */
	while (g_prd_count) {
		/*
		 * Check if this is a read or write operation.
		 */
		if (hwif->sg_dma_direction == PCI_DMA_FROMDEVICE)
			/*
			 * Set up buffers for a read op.
			 */
			temp = ep93xx_dma_add_buffer(hwif->hw.dma,
						     hwif->dma_base,
						     hwif->dmatable_cpu[0],
						     hwif->dmatable_cpu[1], 0,
						     g_prd_count);
		else
			/*
			 * Set up buffers for a write op.
			 */
			temp = ep93xx_dma_add_buffer(hwif->hw.dma,
						     hwif->dmatable_cpu[0],
						     hwif->dma_base,
						     hwif->dmatable_cpu[1], 0,
						     g_prd_count);

		/*
		 * Add a buffer to the dma interface.
		 */
		if (temp != 0)
			break;
		hwif->dmatable_cpu += 2;

		/*
		 * Decrement the count of dmatable entries
		 */
		g_prd_count--;
	}

	/*
	 * See if all of the buffers have been returned.
	 */
	if (g_prd_returned >= g_prd_total) {
		/*
		 * See if the IDE device is still requesting data.
		 */
		if (inl(IDECR) & IDECtrl_DMARQ) {
			/*
			 * Indicate that an error has occurred during this
			 * transfer.
			 */
			g_bad = 1;

			/*
			 * Add another word to the DMA to try and satisfy the
			 * device's request.
			 */
			if (hwif->sg_dma_direction == PCI_DMA_FROMDEVICE)
				ep93xx_dma_add_buffer(hwif->hw.dma,
						      hwif->dma_base,
						      virt_to_bus(&g_garbage),
						      4, 0, 0);
			else
				ep93xx_dma_add_buffer(hwif->hw.dma,
						      virt_to_bus(&g_garbage),
						      hwif->dma_base,
						      4, 0, 0);
		}
	}
}

/*****************************************************************************
 *
 *  ep93xx_dma_timer_expiry()
 *
 *
 *	dma_timer_expiry	-	handle a DMA timeout
 *	@drive: Drive that timed out
 *
 *	An IDE DMA transfer timed out. In the event of an error we ask
 *	the driver to resolve the problem, if a DMA transfer is still
 *	in progress we continue to wait (arguably we need to add a
 *	secondary 'I dont care what the drive thinks' timeout here)
 *	Finally if we have an interrupt we let it complete the I/O.
 *	But only one time - we clear expiry and if it's still not
 *	completed after WAIT_CMD, we error and retry in PIO.
 *	This can occur if an interrupt is lost or due to hang or bugs.
 *
 *
 ****************************************************************************/
static int
ep93xx_idedma_timer_expiry(ide_drive_t *drive)
{
	ide_hwif_t *hwif = HWIF(drive);
	u8 dev_stat	 = hwif->INB(IDE_ALTSTATUS_REG);
	u8 irq_stat	 = inl(IDECR) & IDECtrl_INTRQ;

	DPRINTK(KERN_WARNING "%s: dma_timer_expiry: dev status == 0x%02x,irq= %d\n",
		drive->name, dev_stat, irq_stat);

	/*
	 * Clear the expiry handler in case we decide to wait more,
	 * next time timer expires it is an error
	 */
	HWGROUP(drive)->expiry = NULL;

	/*
	 * If the interrupt is asserted, call the handler.
	 */
	if (irq_stat)
		HWGROUP(drive)->handler(drive);

	/*
	 * Check if the busy bit or the drq bit is set, indicating that
	 * a dma transfer is still active, or the IDE interrupt is asserted.
	 */
	if ( (dev_stat & 0x80) || (dev_stat & 0x08) || irq_stat)
		return WAIT_CMD;

	/*
	 * the device is not busy and the interrupt is not asserted, so check
	 * if there's an error.
	 */
	if (dev_stat & 0x01)	/* ERROR */
		return -1;

	return 0;	/* Unknown status -- reset the bus */
}

/*****************************************************************************
 *
 * ep93xx_config_ide_device()
 *
 * This function sets up the ep93xx ide device for a dma transfer by first
 * probing to find the best dma mode supported by the device.
 *
 * Returns a 0 for success, and a 1 otherwise.
 *
 ****************************************************************************/
static unsigned int
ep93xx_config_ide_device(ide_drive_t *drive)
{
	byte transfer = 0;

	DPRINTK("%s: ep93xx_config_ide_device\n", drive->name);

	/*
	 * Determine the best transfer speed supported.
	 */
	transfer = ide_dma_speed(drive, 2);

	/*
	 * Do not use MW DMA modes above 0 on ATAPI devices since they excite a
	 * chip bug.
	 */
	if ((drive->media != ide_disk) &&
	    ((transfer == XFER_MW_DMA_2) || (transfer == XFER_MW_DMA_1)))
		transfer = XFER_MW_DMA_0;

	/*
	 * Do nothing if a DMA mode is not supported.
	 */
	if (transfer == 0)
		return HWIF(drive)->ide_dma_off(drive);

	/*
	 * Configure the drive.
	 */
	if (ide_config_drive_speed(drive, transfer) == 0) {
		/*
		 * Hold on to this value for use later.
		 */
		drive->current_speed = transfer;

		/*
		 * Success, so turn on DMA.
		 */
		return HWIF(drive)->ide_dma_on(drive);
	} else
		return HWIF(drive)->ide_dma_off(drive);
}

/*****************************************************************************
 *
 *  ep93xx_set_pio()
 *
 *  Configures the ep93xx controller for a PIO mode transfer.
 *
 ****************************************************************************/
static void
ep93xx_set_pio(void)
{
	DPRINTK("ep93xx_set_pio\n");

	/*
	 * Clear the MDMA and UDMA operation registers.
	 */
	outl(0, IDEMDMAOP);
	outl(0, IDEUDMAOP);

	/*
	 * Reset the UDMA state machine.
	 */
	outl(IDEUDMADebug_RWOE | IDEUDMADebug_RWPTR | IDEUDMADebug_RWDR |
	     IDEUDMADebug_RROE | IDEUDMADebug_RRPTR | IDEUDMADebug_RRDR,
	     IDEUDMADEBUG);
	outl(0, IDEUDMADEBUG);

	/*
	 * Enable PIO mode of operation.
	 */
	outl(IDECfg_PIO | IDECfg_IDEEN | (4 << IDECfg_MODE_SHIFT) |
	     (1 << IDECfg_WST_SHIFT), IDECFG);
}

/*****************************************************************************
 *
 *  ep93xx_rwproc()
 *
 *  Initializes the ep93xx IDE controller interface with the transfer type,
 *  transfer mode, and transfer direction.
 *
 ****************************************************************************/
static void
ep93xx_rwproc(ide_drive_t *drive, int action)
{
	DPRINTK("%s: ep93xx_rwproc\n", drive->name);

	/*
	 * Configure the IDE controller for the specified transfer mode.
	 */
	switch (drive->current_speed)
	{
		/*
		 * Configure for an MDMA operation.
		 */
		case XFER_MW_DMA_0:
		case XFER_MW_DMA_1:
		case XFER_MW_DMA_2:
			outl(((drive->current_speed & 3) << IDECfg_MODE_SHIFT) |
			     IDECfg_MDMA | IDECfg_IDEEN, IDECFG);
			outl(action ? IDEMDMAOp_RWOP : 0, IDEMDMAOP);
			outl(inl(IDEMDMAOP) | IDEMDMAOp_MEN, IDEMDMAOP);
			break;

		/*
		 * Configure for a UDMA operation.
		 */
		case XFER_UDMA_0:
		case XFER_UDMA_1:
		case XFER_UDMA_2:
		case XFER_UDMA_3:
		case XFER_UDMA_4:
			outl(((drive->current_speed & 7) << IDECfg_MODE_SHIFT) |
			     IDECfg_UDMA | IDECfg_IDEEN, IDECFG);
			outl(action ? IDEUDMAOp_RWOP : 0, IDEUDMAOP);
			outl(inl(IDEUDMAOP) | IDEUDMAOp_UEN, IDEUDMAOP);
			break;

		default:
			break;
	}
}

/*****************************************************************************
 *
 * ep93xx_ide_dma_check()
 *
 * This function determines if the device supports dma transfers, and if it
 * does, then enables dma.
 *
 ****************************************************************************/
static int
ep93xx_ide_dma_check(ide_drive_t *drive)
{
	ide_hwif_t * hwif = HWIF(drive);
	struct hd_driveid *id = drive->id;

	DPRINTK("%s: ep93xx_ide_dma_check\n", drive->name);

	if (!id || !(id->capability & 1) || !hwif->autodma) {
		/*
		 * Disable dma for this drive.
		 */
		hwif->ide_dma_off_quietly(drive);
		return 1;
	}

	/*
	 * Consult the list of known "bad" drives
	 */
	if (in_drive_list(id, drive_blacklist)) {
		printk("%s: Disabling DMA for %s (blacklisted)\n",
			   drive->name, id->model);

		/*
		 * Disable dma for this drive.
		 */
		hwif->ide_dma_off_quietly(drive);
		return 1;
	}

	/*
	 * Check if the drive supports multiword dma or udma modes.
	 * If it does, then set the device up for that
	 * type of dma transfer, and call ep93xx_ide_dma_on.
	 */
	return ep93xx_config_ide_device(drive);
}

/*****************************************************************************
 *
 * ep93xx_ide_dma_host_off()
 *
 * This function disables dma for the host.
 *
 ****************************************************************************/
static int
ep93xx_ide_dma_host_off(ide_drive_t *drive)
{
	ide_hwif_t *hwif = HWIF(drive);

	/*
	 * TODO: what's to be done here?
	 */
	DPRINTK("%s: ep93xx_ide_dma_host_off\n", drive->name);

	/*
	 * Release the dma channel and all memory allocated for dma
	 * purposes.
	 */
	ep93xx_ide_release_dma(hwif);

	/*
	 * Success.
	 */
	return 0;
}

/*****************************************************************************
 *
 * ep93xx_ide_dma_off_quietly()
 *
 * This function, without announcing it,  disables dma for the device.
 *
 ****************************************************************************/
static int
ep93xx_ide_dma_off_quietly(ide_drive_t *drive)
{
	DPRINTK("%s: ep93xx_ide_dma_off_quietly\n", drive->name);

	/*
	 * Clear the using_dma field to indicate that dma is disabled
	 * for this drive.
	 */
	drive->using_dma = 0;

	/*
	 * Disable dma on the host side.
	 */
	return HWIF(drive)->ide_dma_host_off(drive);
}

/*****************************************************************************
 *
 * ep93xx_ide_dma_off()
 *
 * This function disables dma for the device.
 *
 ****************************************************************************/
static int
ep93xx_ide_dma_off(ide_drive_t *drive)
{
	DPRINTK("%s: ep93xx_ide_dma_off\n", drive->name);

	return HWIF(drive)->ide_dma_off_quietly(drive);
}

/*****************************************************************************
 *
 * ep93xx_ide_dma_on()
 *
 * This function enables dma for the device.
 *
 ****************************************************************************/
static int
ep93xx_ide_dma_on(ide_drive_t *drive)
{
	DPRINTK("%s: ep93xx_ide_dma_on\n", drive->name);

	/*
	 * Set the using_dma field to indicate that dma is enabled.
	 */
	drive->using_dma = 1;

	/*
	 * Enable DMA on the host side.
	 */
	return HWIF(drive)->ide_dma_host_on(drive);
}

/*****************************************************************************
 *
 * ep93xx_ide_dma_host_on()
 *
 * This function enables dma for the device.
 *
 ****************************************************************************/
static int
ep93xx_ide_dma_host_on(ide_drive_t *drive)
{
	DPRINTK("%s: ep93xx_ide_dma_host_on\n", drive->name);

	if (drive->using_dma)
		return 0;
	return 1;
}

/*****************************************************************************
 *
 * ep93xx_ide_dma_read()
 *
 * This function sets up a dma read operation.
 *
 ****************************************************************************/
static int
ep93xx_ide_dma_read(ide_drive_t *drive)
{
	u8 lba48 = (drive->addressing == 1) ? 1 : 0;
	struct request *rq = HWGROUP(drive)->rq;
	unsigned int flags, i;
	ide_hwif_t *hwif = HWIF(drive);
	task_ioreg_t command = WIN_NOP;

	DPRINTK("%s: ep93xx_ide_dma_read\n", drive->name);
	DPRINTK("    %ld, %ld\n", HWGROUP(drive)->rq->sector,
		HWGROUP(drive)->rq->nr_sectors);

	/*
	 * Check if we are already transferring on this dma channel.
	 */
	if (hwif->sg_dma_active || drive->waiting_for_dma) {
		DPRINTK("%s: dma_read: dma already active \n", drive->name);
		return 1;
	}

	/*
	 * Compute the array index used for this request.
	 */
	i = drive->name[2] == 'a' ? 0 : 2;

	/*
	 * See if this request matches the most recent request, and that the
	 * most recent request ended in error.
	 */
	if (g_bad && (g_cmd == READ) && (g_drive == drive) &&
	    (g_sector == HWGROUP(drive)->rq->sector) &&
	    (g_nr_sectors == HWGROUP(drive)->rq->nr_sectors)) {
		g_cur_retries++;
		if (g_cur_retries > g_max_retries[i])
			g_max_retries[i]++;
		if (g_cur_retries == RETRIES_PER_TRANSFER) {
			g_bad = 0;
			return 1;
		}
	} else {
		g_cur_retries = 0;
		g_cmd = READ;
		g_drive = drive;
		g_sector = HWGROUP(drive)->rq->sector;
		g_nr_sectors = HWGROUP(drive)->rq->nr_sectors;
	}

	/*
	 * Save information about this transfer.
	 */
	g_xfers[i]++;
	g_sectors[i] += HWGROUP(drive)->rq->nr_sectors;

	/*
	 * Indicate that we're waiting for dma.
	 */
	drive->waiting_for_dma = 1;

	/*
	 * Configure DMA M2M channel flags for a source address hold, h/w
	 * initiated P2M transfer.
	 */
	flags = (SOURCE_HOLD | TRANSFER_MODE_HW_P2M);

	if (drive->current_speed & 0x20) {
		flags |= (WS_IDE_MDMA_READ << WAIT_STATES_SHIFT);

		/*
		 * MDMA data register address.
		 */
		hwif->dma_base = IDEMDMADATAIN - IO_BASE_VIRT + IO_BASE_PHYS;
	} else {
		flags |= (WS_IDE_UDMA_READ << WAIT_STATES_SHIFT);
		/*
		 * UDMA data register address.
		 */
		hwif->dma_base = IDEUDMADATAIN - IO_BASE_VIRT + IO_BASE_PHYS;
	}

	/*
	 * Configure the dma interface for this IDE operation.
	 */
	if (ep93xx_dma_config(hwif->hw.dma, 0, flags, ep93xx_ide_callback,
			      (unsigned int)drive) != 0) {
		DPRINTK("%s: ep93xx_ide_dma_read: ERROR- dma config failed",
				drive->name);
		drive->waiting_for_dma = 0;
		/*
		 * Fail.
		 */
		return 1;
	}

	/*
	 * Build the table of dma-able buffers.
	 */
	if (!(g_prd_count = ide_build_dmatable(drive))) {
		DPRINTK("%s: ep93xx_ide_dma_read: ERROR- failed to build dma table",
			drive->name);
		drive->waiting_for_dma = 0;
		/*
		 * Fail, try PIO instead of DMA
		 */
		return 1;
	}

	/*
	 * Indicate that the scatter gather is active.
	 */
	hwif->sg_dma_active = 1;

	/*
	 * test stuff
	 */
	g_prd_total = g_prd_count;
	g_prd_returned = 0;

	DPRINTK("%d buffers\n", g_prd_total);

	/*
	 * Prepare the dma interface with some buffers from the
	 * dma_table.
	 */
	do {
		/*
		 * Add a buffer to the dma interface.
		 */
		if (ep93xx_dma_add_buffer(hwif->hw.dma, hwif->dma_base,
					  hwif->dmatable_cpu[0],
					  hwif->dmatable_cpu[1], 0,
					  g_prd_count) != 0)
			break;
		hwif->dmatable_cpu += 2;

		/*
		 * Decrement the count of dmatable entries
		 */
		g_prd_count--;
	} while (g_prd_count);

	/*
	 * Nothing further is required if this is not a ide_disk (i.e. an ATAPI
	 * device).
	 */
	if (drive->media != ide_disk)
		return 0;

	/*
	 * Determine the command to be sent to the device.
	 */
	command = (lba48) ? WIN_READDMA_EXT : WIN_READDMA;
	if (rq->cmd == IDE_DRIVE_TASKFILE) {
		ide_task_t *args = rq->special;
		command = args->tfRegister[IDE_COMMAND_OFFSET];
	}

	/*
	 * Send the read command to the device.
	 */
	ide_execute_command(drive, command, &ep93xx_ide_dma_intr, 2*WAIT_CMD,
			    &ep93xx_idedma_timer_expiry);

	/*
	 * initiate the dma transfer.
	 */
	return hwif->ide_dma_begin(drive);
}

/*****************************************************************************
 *
 * ep93xx_ide_dma_first_read()
 *
 * This function handles the very first dma read operation.
 *
 ****************************************************************************/
static int
ep93xx_ide_dma_first_read(ide_drive_t *drive)
{
	ide_hwif_t *hwif = HWIF(drive);

	/*
	 * Switch the hwif to the real read routine.
	 */
	hwif->ide_dma_read = ep93xx_ide_dma_read;

	/*
	 * Register a proc entry for the statistics that we gather.
	 */
#ifdef CONFIG_PROC_FS
	create_proc_info_entry("ide/ep93xx", 0, 0, ep93xx_get_info);
#endif

	/*
	 * Do the actual read.
	 */
	return hwif->ide_dma_read(drive);
}

/*****************************************************************************
 *
 * ep93xx_ide_dma_write()
 *
 * This function sets up a dma write operation.
 *
 ****************************************************************************/
static int
ep93xx_ide_dma_write(ide_drive_t *drive)
{
	u8 lba48 = (drive->addressing == 1) ? 1 : 0;
	struct request *rq = HWGROUP(drive)->rq;
	unsigned int flags, i;
	ide_hwif_t *hwif = HWIF(drive);
	task_ioreg_t command = WIN_NOP;

	DPRINTK("%s: ep93xx_ide_dma_write\n", drive->name);

	/*
	 * Check if we are already transferring on this dma channel.
	 */
	if (hwif->sg_dma_active || drive->waiting_for_dma) {
		DPRINTK("%s: dma_write - dma is already active \n",
			drive->name);
		return 1;
	}

	/*
	 * Compute the array index used for this request.
	 */
	i = drive->name[2] == 'a' ? 1 : 3;

	/*
	 * See if this request matches the most recent request, and that the
	 * most recent request ended in error.
	 */
	if (g_bad && (g_cmd == WRITE) && (g_drive == drive) &&
	    (g_sector == HWGROUP(drive)->rq->sector) &&
	    (g_nr_sectors == HWGROUP(drive)->rq->nr_sectors)) {
		g_cur_retries++;
		if (g_cur_retries > g_max_retries[i])
			g_max_retries[i]++;
		if (g_cur_retries == RETRIES_PER_TRANSFER) {
			g_bad = 0;
			return 1;
		}
	} else {
		g_cur_retries = 0;
		g_cmd = WRITE;
		g_drive = drive;
		g_sector = HWGROUP(drive)->rq->sector;
		g_nr_sectors = HWGROUP(drive)->rq->nr_sectors;
	}

	/*
	 * Save information about this transfer.
	 */
	g_xfers[i]++;
	g_sectors[i] += HWGROUP(drive)->rq->nr_sectors;

	/*
	 * Indicate that we're waiting for dma.
	 */
	drive->waiting_for_dma = 1;

	/*
	 * Configure DMA M2M channel flags for a destination address
	 * hold, h/w initiated M2P transfer.
	 */
	flags = (DESTINATION_HOLD | TRANSFER_MODE_HW_M2P);

	/*
	 * Determine if we need the MDMA or UDMA data register.
	 */
	if (drive->current_speed & 0x20) {
		flags |= (WS_IDE_MDMA_WRITE << WAIT_STATES_SHIFT);

		/*
		 * MDMA data register address.
		 */
		hwif->dma_base = IDEMDMADATAOUT - IO_BASE_VIRT + IO_BASE_PHYS;
	} else {
		flags |= (WS_IDE_UDMA_WRITE << WAIT_STATES_SHIFT);

		/*
		 * UDMA data register address.
		 */
		hwif->dma_base = IDEUDMADATAOUT - IO_BASE_VIRT + IO_BASE_PHYS;
	}

	/*
	 * Configure the dma interface for this IDE operation.
	 */
	if (ep93xx_dma_config(hwif->hw.dma, 0, flags, ep93xx_ide_callback,
			      (unsigned int)drive) != 0) {
		drive->waiting_for_dma = 0;
		return 1;
	}

	/*
	 * Build the table of dma-able buffers.
	 */
	if (!(g_prd_count = ide_build_dmatable(drive))) {
		drive->waiting_for_dma = 0;
		/*
		 * Fail, try PIO instead of DMA
		 */
		return 1;
	}

	/*
	 * Indicate that we're waiting for dma.
	 */
	hwif->sg_dma_active = 1;

	/*
	 * test stuff
	 */
	g_prd_total = g_prd_count;
	g_prd_returned = 0;

	/*
	 * Prepare the dma interface with some buffers from the
	 * dma_table.
	 */
	do {
		/*
		 * Add a buffer to the dma interface.
		 */
		if (ep93xx_dma_add_buffer(hwif->hw.dma,
					  hwif->dmatable_cpu[0],
					  hwif->dma_base,
					  hwif->dmatable_cpu[1], 0,
					  g_prd_count) != 0)
			break;
		hwif->dmatable_cpu += 2;

		/*
		 * Decrement the count of dmatable entries
		 */
		g_prd_count--;
	} while (g_prd_count);

	/*
	 * Nothing further is required if this is not a ide_disk (i.e. an ATAPI
	 * device).
	 */
	if (drive->media != ide_disk)
		return 0;

	/*
	 * Determine the command to be sent to the device.
	 */
	command = (lba48) ? WIN_WRITEDMA_EXT : WIN_WRITEDMA;
	if (rq->cmd == IDE_DRIVE_TASKFILE) {
		ide_task_t *args = rq->special;
		command = args->tfRegister[IDE_COMMAND_OFFSET];
	}

	/*
	 * Send the write dma command to the device.
	 */
	ide_execute_command(drive, command, &ep93xx_ide_dma_intr, 2*WAIT_CMD,
			    &ep93xx_idedma_timer_expiry);

	/*
	 * initiate the dma transfer.
	 */
	return hwif->ide_dma_begin(drive);
}

/*****************************************************************************
 *
 * ep93xx_ide_dma_begin()
 *
 * This function initiates a dma transfer.
 *
 ****************************************************************************/
static int
ep93xx_ide_dma_begin(ide_drive_t *drive)
{
	ide_hwif_t *hwif = HWIF(drive);

	DPRINTK("%s: ep93xx_ide_dma_begin\n", drive->name);

	/*
	 * The transfer is not bad yet.
	 */
	g_bad = 0;

	/*
	 * Configure the ep93xx ide controller for a dma operation.
	 */
	if (HWGROUP(drive)->rq->cmd == READ)
		ep93xx_rwproc(drive, 0);
	else
		ep93xx_rwproc(drive, 1);

	/*
	 * Start the dma transfer.
	 */
	ep93xx_dma_start(hwif->hw.dma, 1, NULL);

	return 0;
}

/*****************************************************************************
 *
 * ep93xx_ide_dma_end()
 *
 * This function performs any tasks needed to cleanup after a dma transfer.
 * Returns 1 if an error occured, and 0 otherwise.
 *
 ****************************************************************************/
static int
ep93xx_ide_dma_end(ide_drive_t *drive)
{
	ide_hwif_t * hwif = HWIF(drive);
	int i, stat;

	DPRINTK("%s: ep93xx_ide_dma_end\n", drive->name);

	/*
	 * See if there is any data left in UDMA FIFOs.  For a read, first wait
	 * until either the DMA is done or the FIFO is empty.  If there is any
	 * residual data in the FIFO, there was an error in the transfer.
	 */
	if (HWGROUP(drive)->rq->cmd == READ) {
		do {
			i = inl(IDEUDMARFST);
		} while (!ep93xx_dma_is_done(hwif->hw.dma) &&
			 ((i & 15) != ((i >> 4) & 15)));
		udelay(1);
	}
	else
		i = inl(IDEUDMAWFST);
	if ((i & 15) != ((i >> 4) & 15))
		g_bad = 1;
            
	/*
	 * See if all of the buffers were returned by the DMA driver.  If not,
	 * then a transfer error has occurred.
	 */
	if (!ep93xx_dma_is_done(hwif->hw.dma))
		g_bad = 1;

	/*
	 * Put the dma interface into pause mode.
	 */
	ep93xx_dma_pause(hwif->hw.dma, 1, 0);
	ep93xx_dma_flush(hwif->hw.dma);

	/*
	 * Enable PIO mode on the IDE interface.
	 */
	ep93xx_set_pio();

	/*
	 * Indicate there's no dma transfer currently in progress.
	 */
	hwif->sg_dma_active = 0;
	drive->waiting_for_dma = 0;

	/*
	 * Get status from the ide device.
	 */
	stat = HWIF(drive)->INB(IDE_STATUS_REG);

	/*
	 * If this is an ATAPI device and it reported an error, then simply
	 * return a DMA success.
	 */
	if ((drive->media != ide_disk) && (stat & ERR_STAT))
		return 0;

	/*
	 * See if a CRC error occurred.  If so, then a transfer error has
	 * occurred.
	 */
	if ((stat & ERR_STAT) && (HWIF(drive)->INB(IDE_ERROR_REG) & ICRC_ERR))
		g_bad = 1;

	/*
	 * Compute the array index used for this request.
	 */
	i = HWGROUP(drive)->rq->cmd == READ ? 0 : 1;
	i += drive->name[2] == 'a' ? 0 : 2;

	/*
	 * Fail if an error occurred during the DMA.
	 */
	if (g_bad) {
		/*
		 * Increment the number of retries for this request, along with
		 * the number of consecutive errors for this drive.
		 */
		g_retries[i]++;
		g_errors[i]++;

		/*
		 * See if the maximum number of consective errors has occurred.
		 */
		if (g_errors[i] == FAILURES_BEFORE_BACKOFF) {
			/*
			 * Drop down to the next lowest transfer rate, or PIO
			 * if already at the bottom.
			 */
			switch (drive->current_speed) {
			case XFER_UDMA_4:
				ide_config_drive_speed(drive, XFER_UDMA_3);
				break;
			case XFER_UDMA_3:
				ide_config_drive_speed(drive, XFER_UDMA_2);
				break;
			case XFER_UDMA_2:
				ide_config_drive_speed(drive, XFER_UDMA_1);
				break;
			case XFER_UDMA_1:
				ide_config_drive_speed(drive, XFER_UDMA_0);
				break;
			case XFER_UDMA_0:
				HWIF(drive)->ide_dma_off(drive);
				break;
			case XFER_MW_DMA_2:
				ide_config_drive_speed(drive, XFER_MW_DMA_1);
				break;
			case XFER_MW_DMA_1:
				ide_config_drive_speed(drive, XFER_MW_DMA_0);
				break;
			case XFER_MW_DMA_0:
				HWIF(drive)->ide_dma_off(drive);
				break;
			}

			/*
			 * Reset the consecutive error counters for this drive.
			 */
			g_errors[i] = 0;
			g_errors[i ^ 1] = 0;
		}

		/*
		 * Fail the transfer.
		 */
		return 1;
	} else
		/*
		 * Success, so reset the consecutive error counter.
		 */
		g_errors[i] = 0;

	/*
	 * Success.
	 */
	return 0;
}

/*****************************************************************************
 *
 * ep93xx_ide_dma_test_irq()
 *
 * This function checks if the IDE interrupt is asserted and returns a
 * 1 if it is, and 0 otherwise..
 *
 ****************************************************************************/
static int
ep93xx_ide_dma_test_irq(ide_drive_t *drive)
{
	DPRINTK("%s: ep93xx_ide_dma_test_irq\n", drive->name);

	if (!drive->waiting_for_dma)
		printk(KERN_WARNING "%s: %s called while not waiting\n",
		       drive->name, __FUNCTION__);

	/*
	 * Return the value of the IDE interrupt bit.
	 */
	return inl(IDECR) & IDECtrl_INTRQ;
}

/*****************************************************************************
 *
 * ep93xx_ide_dma_count()
 *
 ****************************************************************************/
static int
ep93xx_ide_dma_count(ide_drive_t *drive)
{
	DPRINTK("%s: ep93xx_ide_dma_count\n", drive->name);

	return 0;
}

/*****************************************************************************
 *
 * ep93xx_ide_dma_verbose()
 *
 ****************************************************************************/
static int
ep93xx_ide_dma_verbose(ide_drive_t *drive)
{
	DPRINTK("%s: ep93xx_ide_dma_verbose\n", drive->name);

	printk(", %s ", ide_xfer_verbose(drive->current_speed));

	return 1;

}

/*****************************************************************************
 *
 * ep93xx_ide_dma_bad_timeout()
 *
 ****************************************************************************/
static int
ep93xx_ide_dma_timeout(ide_drive_t *drive)
{
	DPRINTK("%s: ep93xx_ide_dma_timeout\n", drive->name);

	if (HWIF(drive)->ide_dma_test_irq(drive))
		return 0;
	return HWIF(drive)->ide_dma_end(drive);
}

/*****************************************************************************
 *
 * ep93xx_ide_dma_lostirq()
 *
 ****************************************************************************/
static int
ep93xx_ide_dma_lostirq(ide_drive_t *drive)
{
	DPRINTK("%s: ep93xx_ide_dma_lostirq\n", drive->name);
	printk("%s: DMA interrupt recovery\n", drive->name);
	return 1;
}

/*****************************************************************************
 *
 *  functions to set up the IDE control register and data register to read
 *  or write a byte of data to/from the specified IDE device register.
 *
 ****************************************************************************/
static void
ep93xx_ide_outb(u8 b, unsigned long addr)
{
	unsigned int uiIDECR;

	/*
	 * Write the address out.
	 */
	uiIDECR = IDECtrl_DIORn | IDECtrl_DIOWn | ((addr & 7) << 2) |
		  (addr >> 10);
	outl(uiIDECR, IDECR);

	/*
	 * Write the data out.
	 */
	outl(b, IDEDATAOUT);

	/*
	 * Toggle the write signal.
	 */
	outl(uiIDECR & ~IDECtrl_DIOWn, IDECR);
	outl(uiIDECR, IDECR);
}

static void
ep93xx_ide_outbsync(ide_drive_t *drive, u8 b, unsigned long addr)
{
	unsigned int uiIDECR;

	/*
	 * Write the address out.
	 */
	uiIDECR = IDECtrl_DIORn | IDECtrl_DIOWn | ((addr & 7) << 2) |
		  (addr >> 10);
	outl(uiIDECR, IDECR);

	/*
	 * Write the data out.
	 */
	outl(b, IDEDATAOUT);

	/*
	 * Toggle the write signal.
	 */
	outl(uiIDECR & ~IDECtrl_DIOWn, IDECR);
	outl(uiIDECR, IDECR);
}

static unsigned char
ep93xx_ide_inb(unsigned long addr)
{
	unsigned int uiIDECR;

	/*
	 * Write the address out.
	 */
	uiIDECR = IDECtrl_DIORn | IDECtrl_DIOWn | ((addr & 7) << 2) |
		  (addr >> 10);
	outl(uiIDECR, IDECR);

	/*
	 * Toggle the read signal.
	 */
	outl(uiIDECR & ~IDECtrl_DIORn, IDECR);
	outl(uiIDECR, IDECR);

	/*
	 * Read the data in.
	 */
	return(inl(IDEDATAIN) & 0xff);
}

/*****************************************************************************
 *
 *  functions to set up the IDE control register and data restister to read
 *  or write 16 bits of data to/from the specified IDE device register.
 *  These functions should only be used when reading/writing data to/from
 *  the data register.
 *
 ****************************************************************************/
static void
ep93xx_ide_outw(u16 w, unsigned long addr)
{
	unsigned int uiIDECR;

	/*
	 * Write the address out.
	 */
	uiIDECR = IDECtrl_DIORn | IDECtrl_DIOWn | ((addr & 7) << 2) |
		  (addr >> 10);
	outl(uiIDECR, IDECR);

	/*
	 * Write the data out.
	 */
	outl(w, IDEDATAOUT);

	/*
	 * Toggle the write signal.
	 */
	outl(uiIDECR & ~IDECtrl_DIOWn, IDECR);
	outl(uiIDECR, IDECR);
}

static u16
ep93xx_ide_inw(unsigned long addr)
{
	unsigned int uiIDECR;

	/*
	 * Write the address out.
	 */
	uiIDECR = IDECtrl_DIORn | IDECtrl_DIOWn | ((addr & 7) << 2) |
		  (addr >> 10);
	outl(uiIDECR, IDECR);

	/*
	 * Toggle the read signal.
	 */
	outl(uiIDECR & ~IDECtrl_DIORn, IDECR);
	outl(uiIDECR, IDECR);

	/*
	 * Read the data in.
	 */
	return(inl(IDEDATAIN) & 0xffff);
}

/*****************************************************************************
 *
 *  functions to read/write a block of data to/from the ide device using
 *  PIO mode.
 *
 ****************************************************************************/
static void
ep93xx_ide_insw(unsigned long addr, void *buf, u32 count)
{
	unsigned short *data = (unsigned short *)buf;

	/*
	 * Read in data from the data register 16 bits at a time.
	 */
	for (; count; count--)
		*data++ = ep93xx_ide_inw(addr);
}

static void
ep93xx_ide_outsw(unsigned long addr, void *buf, u32 count)
{
	unsigned short *data = (unsigned short *)buf;

	/*
	 * Write out data to the data register 16 bits at a time.
	 */
	for (; count; count--)
		ep93xx_ide_outw(*data++, addr);
}

static void
ep93xx_ata_input_data(ide_drive_t *drive, void *buffer, u32 count)
{
	/*
	 * Read in the specified number of half words from the ide interface.
	 */
	ep93xx_ide_insw(IDE_DATA_REG, buffer, count << 1);
}

static void
ep93xx_ata_output_data(ide_drive_t *drive, void *buffer, u32 count)
{
	/*
	 * write the specified number of half words from the ide interface
	 * to the ide device.
	 */
	ep93xx_ide_outsw(IDE_DATA_REG, buffer, count << 1);
}

static void
ep93xx_atapi_input_bytes(ide_drive_t *drive, void *buffer, u32 count)
{
	/*
	 * read in the specified number of bytes from the ide interface.
	 */
	ep93xx_ide_insw(IDE_DATA_REG, buffer, (count >> 1) + (count & 1));
}

static void
ep93xx_atapi_output_bytes(ide_drive_t *drive, void *buffer, u32 count)
{
	/*
	 * Write the specified number of bytes from the ide interface
	 * to the ide device.
	 */
	ep93xx_ide_outsw(IDE_DATA_REG, buffer, (count >> 1) + (count & 1));
}

/*****************************************************************************
 *
 * ep93xx_ide_init()
 *
 * This function sets up a pointer to the ep93xx specific ideproc funciton.
 *
 ****************************************************************************/
void
ep93xx_ide_init(void)
{
	DPRINTK("ep93xx_ide_init\n");
	hw_regs_t hw;
	struct hwif_s *hwif;
	unsigned int uiTemp;

	/*
	 * Make sure the GPIO on IDE bits in the DEVCFG register are not set.
	 */
	uiTemp = inl(SYSCON_DEVCFG) & ~(SYSCON_DEVCFG_EonIDE |
					SYSCON_DEVCFG_GonIDE |
					SYSCON_DEVCFG_HonIDE);
	SysconSetLocked( SYSCON_DEVCFG, uiTemp );

	/*
	 * Make sure that MWDMA and UDMA are disabled.
	 */
	outl(0, IDEMDMAOP);
	outl(0, IDEUDMAOP);

	/*
	 * Set up the IDE interface for PIO transfers, using the default PIO
	 * mode.
	 */
	outl(IDECfg_IDEEN | IDECfg_PIO | (4 << IDECfg_MODE_SHIFT) |
	     (1 << IDECfg_WST_SHIFT), IDECFG);

	/*
	 * Setup the ports.
	 */
	ide_init_hwif_ports(&hw, 0x800, 0x406, NULL);

	/*
	 * Get the interrupt.
	 */
	hw.irq = IRQ_EIDE;

	/*
	 * This is the dma channel number assigned to this IDE interface. Until
	 * dma is enabled for this interface, we set it to NO_DMA.
	 */
	hw.dma = NO_DMA;

	/*
	 * Register the IDE interface, an ide_hwif_t pointer is passed in,
	 * which will get filled in with the hwif pointer for this interface.
	 */
	ide_register_hw(&hw, &hwif);

	/*
	 *  Set up the HW interface function pointers with the ep93xx specific
	 *  function.
	 */
	hwif->ata_input_data = ep93xx_ata_input_data;
	hwif->ata_output_data = ep93xx_ata_output_data;
	hwif->atapi_input_bytes = ep93xx_atapi_input_bytes;
	hwif->atapi_output_bytes = ep93xx_atapi_output_bytes;

	hwif->OUTB = ep93xx_ide_outb;
	hwif->OUTBSYNC = ep93xx_ide_outbsync;
	hwif->OUTW = ep93xx_ide_outw;
	hwif->OUTSW = ep93xx_ide_outsw;

	hwif->INB = ep93xx_ide_inb;
	hwif->INW = ep93xx_ide_inw;
	hwif->INSW = ep93xx_ide_insw;

	/*
	 * Only enable IDE DMA on rev D1 and later chips.
	 */
	if ((inl(SYSCON_CHIPID) >> 28) > 3) {
		DPRINTK("ep93xx_ide_init- dma enabled \n");

		/*
		 * Allocate memory for the DMA table.
		 */
		g_dma_table_base = kmalloc(PRD_ENTRIES * PRD_BYTES,
					   GFP_KERNEL);

		/*
		 * Check if we allocated memory for dma
		 */
		if (g_dma_table_base == NULL) {
			printk("%s: SG-DMA disabled, UNABLE TO ALLOCATE DMA TABLES\n",
			       hwif->name);
			return;
		}

		/*
		 * Allocate memory for the scatterlist structures.
		 */
		hwif->sg_table = kmalloc(sizeof(struct scatterlist) *
					 PRD_ENTRIES, GFP_KERNEL);

		/*
		 * Check if we allocated the memory we expected to.
		 */
		if (hwif->sg_table == NULL) {
			/*
			 *  Fail, so clean up.
			 */
			kfree(g_dma_table_base);
			printk("%s: SG-DMA disabled, UNABLE TO ALLOCATE DMA TABLES\n",
			       hwif->name);
			return;
		}

		/*
		 * Init the dma handle to 0.  This field is used to hold a
		 * handle to the dma instance.
		 */
		hwif->hw.dma = 0;

		/*
		 * Open an instance of the ep93xx dma interface.
		 */
		if (ep93xx_dma_request(&hwif->hw.dma, hwif->name, DMA_IDE) !=
		    0) {
			/*
			 * Fail, so clean up.
			 */
			kfree(g_dma_table_base);
			kfree(hwif->sg_table);
			printk("%s: SG-DMA disabled, unable to allocate DMA channel.\n",
			       hwif->name);
			return;
		}

		/*
		 * Now that we've got a dma channel allocated, set up the rest
		 * of the dma specific stuff.
		 */
		DPRINTK("\n ide init- dma channel allocated: %d \n", hwif->hw.dma);

		/*
		 * Enable dma support for atapi devices.
		 */
		hwif->atapi_dma			= 1;

		hwif->ultra_mask		= 0x07;
		hwif->mwdma_mask		= 0x00;
		hwif->swdma_mask		= 0x00;

		hwif->speedproc			= NULL;
		hwif->autodma			= 1;

		hwif->ide_dma_check		= ep93xx_ide_dma_check;
		hwif->ide_dma_host_off		= ep93xx_ide_dma_host_off;
		hwif->ide_dma_off_quietly	= ep93xx_ide_dma_off_quietly;
		hwif->ide_dma_off		= ep93xx_ide_dma_off;
		hwif->ide_dma_host_on		= ep93xx_ide_dma_host_on;
		hwif->ide_dma_on		= ep93xx_ide_dma_on;
		hwif->ide_dma_read		= ep93xx_ide_dma_first_read;
		hwif->ide_dma_write		= ep93xx_ide_dma_write;
		hwif->ide_dma_count		= ep93xx_ide_dma_count;
		hwif->ide_dma_begin		= ep93xx_ide_dma_begin;
		hwif->ide_dma_end		= ep93xx_ide_dma_end;
		hwif->ide_dma_test_irq		= ep93xx_ide_dma_test_irq;
		hwif->ide_dma_verbose		= ep93xx_ide_dma_verbose;
		hwif->ide_dma_timeout		= ep93xx_ide_dma_timeout;
		hwif->ide_dma_lostirq		= ep93xx_ide_dma_lostirq;
	}
}
