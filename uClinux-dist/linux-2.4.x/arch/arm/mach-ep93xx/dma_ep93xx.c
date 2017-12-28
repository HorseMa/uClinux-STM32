/******************************************************************************
 * arch/arm/mach-ep9312/dma_ep93xx.c
 *
 * Support functions for the ep93xx internal DMA channels.
 * (see also Documentation/arm/ep93xx/dma.txt)
 *
 * Copyright (C) 2003  Cirrus Logic
 *
 * A large portion of this file is based on the dma api implemented by
 * Nicolas Pitre, dma-sa1100.c, copyrighted 2000.
 *
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
 ****************************************************************************/
#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/delay.h>

#include <asm/system.h>
#include <asm/irq.h>
#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/dma.h>
#include <asm/mach/dma.h>
#include "dma_ep93xx.h"

/*****************************************************************************
 *
 * Debugging macros
 *
 ****************************************************************************/
#undef DEBUG
//#define DEBUG   1
#ifdef DEBUG
#define DPRINTK( fmt, arg... )  printk( fmt, ##arg )
#else
#define DPRINTK( fmt, arg... )
#endif

/*****************************************************************************
 *
 * static global variables
 *
 ****************************************************************************/
ep93xx_dma_t dma_chan[MAX_EP93XX_DMA_CHANNELS];

/*
 *  lock used to protect the list of dma channels while searching for a free
 *  channel during dma_request.
 */
static spinlock_t dma_list_lock;

/*****************************************************************************
 *
 *  Internal DMA processing functions.
 *
 ****************************************************************************/
/*****************************************************************************
 *
 *  get_dma_channel_from_handle()
 *
 *  If Handle is valid, returns the DMA channel # (0 to 9 for channels 1-10)
 *  If Handle is not valid, returns -1.
 *
 ****************************************************************************/
static int
dma_get_channel_from_handle(int handle)
{
	int channel;

	/*
	 *  Get the DMA channel # from the handle.
	 */
	channel = ((int)handle & DMA_HANDLE_SPECIFIER_MASK) >> 28;

	/*
	 *  See if this is a valid handle.
	 */
	if (dma_chan[channel].last_valid_handle != (int)handle) {
		DPRINTK("DMA ERROR - invalid handle 0x%x \n", handle);
		return(-1);
	}

	/*
	 *  See if this instance is still open
	 */
	if (!dma_chan[channel].ref_count )
		return(-1);

	return(channel);
}

static void dma_m2m_transfer_done(ep93xx_dma_t *dma)
{
	unsigned int uiCONTROL;
	unsigned int M2M_reg_base = dma->reg_base;
	unsigned int read_back;

	DPRINTK("1  ");

	outl( 0, M2M_reg_base+M2M_OFFSET_INTERRUPT );

	if (dma->total_buffers) {
		/*
		 * The current_buffer has already been tranfered, so add the
		 * byte count to the total_bytes field.
		 */
		dma->total_bytes = dma->total_bytes +
			dma->buffer_queue[dma->current_buffer].size;

		/*
		 * Mark the current_buffer as used.
		 */
		dma->buffer_queue[dma->current_buffer].used = TRUE;

		/*
		 * Increment the used buffer counter
		 */
		dma->used_buffers++;

		DPRINTK("#%d", dma->current_buffer);

		/*
		 * Increment the current_buffer
		 */
		dma->current_buffer = (dma->current_buffer + 1) %
				      MAX_EP93XX_DMA_BUFFERS;

		/*
		 * check if there's a new buffer to transfer.
		 */
		if (dma->new_buffers && dma->xfer_enable) {
			/*
			 * We have a new buffer to transfer so program in the
			 * buffer values.  Since a STALL interrupt was
			 * triggered, we program the buffer descriptor 0
			 *
			 * Set the SAR_BASE/DAR_BASE/BCR registers with values
			 * from the next buffer in the queue.
			 */
			outl( dma->buffer_queue[dma->current_buffer].source,
			      M2M_reg_base + M2M_OFFSET_SAR_BASE0 );

			outl( dma->buffer_queue[dma->current_buffer].dest,
			      M2M_reg_base + M2M_OFFSET_DAR_BASE0 );

			outl( dma->buffer_queue[dma->current_buffer].size,
			      M2M_reg_base + M2M_OFFSET_BCR0 );

			DPRINTK("SAR_BASE0 - 0x%x\n", dma->buffer_queue[dma->current_buffer].source);
			DPRINTK("DAR_BASE0 - 0x%x\n", dma->buffer_queue[dma->current_buffer].dest);
			DPRINTK("BCR0 - 0x%x\n", dma->buffer_queue[dma->current_buffer].size);

			/*
			 * Decrement the new buffer counter
			 */
			dma->new_buffers--;

			/*
			 * If there's a second new buffer, we program the
			 * second buffer descriptor.
			 */
			if (dma->new_buffers) {
				outl( dma->buffer_queue[(dma->current_buffer + 1) %
							MAX_EP93XX_DMA_BUFFERS].source,
				      M2M_reg_base+M2M_OFFSET_SAR_BASE1 );

				outl( dma->buffer_queue[(dma->current_buffer + 1) %
							MAX_EP93XX_DMA_BUFFERS].dest,
				      M2M_reg_base+M2M_OFFSET_DAR_BASE1 );

				outl( dma->buffer_queue[(dma->current_buffer + 1) %
							MAX_EP93XX_DMA_BUFFERS].size,
				      M2M_reg_base+M2M_OFFSET_BCR1 );

				uiCONTROL = inl(M2M_reg_base+M2M_OFFSET_CONTROL);
				uiCONTROL |= CONTROL_M2M_NFBINTEN;
				outl( uiCONTROL, M2M_reg_base+M2M_OFFSET_CONTROL );

				dma->new_buffers--;
			}
		} else {
			DPRINTK("2 \n");
			/*
			 * There's a chance we setup both buffer descriptors,
			 * but didn't service the NFB quickly enough, causing
			 * the channel to transfer both buffers, then enter the
			 * stall state.  So, we need to be able to process the
			 * second buffer.
			 */
			if ((dma->used_buffers + dma->new_buffers) < dma->total_buffers)
			{
				DPRINTK("3 ");

				/*
				 * The current_buffer has already been
				 * tranferred, so add the byte count to the
				 * total_bytes field.
				 */
				dma->total_bytes = dma->total_bytes +
					dma->buffer_queue[dma->current_buffer].size;

				/*
				 * Mark the current_buffer as used.
				 */
				dma->buffer_queue[dma->current_buffer].used = TRUE;

				/*
				 * Increment the used buffer counter
				 */
				dma->used_buffers++;

				DPRINTK("#%d", dma->current_buffer);

				/*
				 * Increment the current buffer pointer.
				 */
				dma->current_buffer = (dma->current_buffer + 1) %
						      MAX_EP93XX_DMA_BUFFERS;

			}

			/*
			 * No new buffers to transfer, so disable the channel.
			 */
			uiCONTROL = inl(M2M_reg_base+M2M_OFFSET_CONTROL);
			uiCONTROL &= ~CONTROL_M2M_ENABLE;
			outl( uiCONTROL, M2M_reg_base+M2M_OFFSET_CONTROL );

			/*
			 * Indicate that this channel is in the pause by
			 * starvation state by setting the pause bit to true.
			 */
			dma->pause = TRUE;
		}
	} else {
		/*
		 * No buffers to transfer, or old buffers to mark as used,
		 * so disable the channel
		 */
		uiCONTROL = inl(M2M_reg_base+M2M_OFFSET_CONTROL);
		uiCONTROL &= ~CONTROL_M2M_ENABLE;
		outl( uiCONTROL, M2M_reg_base+M2M_OFFSET_CONTROL );

		/*
		 * Must read the control register back after a write.
		 */
		read_back = inl(M2M_reg_base+M2M_OFFSET_CONTROL);

		/*
		 * Indicate that this channel is in the pause by
		 * starvation state by setting the pause bit to true.
		 */
		dma->pause = TRUE;
	}
}

static void dma_m2m_next_frame_buffer(ep93xx_dma_t *dma)
{
	int loop;
	unsigned int uiCONTROL;
	unsigned int M2M_reg_base = dma->reg_base;

	DPRINTK("5  ");

	if (dma->total_buffers) {
		DPRINTK("6  ");
		/*
		 * The iCurrentBuffer has already been transfered.  so add the
		 * byte count from the current buffer to the total byte count.
		 */
		dma->total_bytes = dma->total_bytes +
			dma->buffer_queue[dma->current_buffer].size;

		/*
		 * Mark the Current Buffer as used.
		 */
		dma->buffer_queue[dma->current_buffer].used = TRUE;

		/*
		 * Increment the used buffer counter
		 */
		dma->used_buffers++;

		DPRINTK("#%d", dma->current_buffer);

		if ((dma->buffer_queue[
		    (dma->current_buffer + 1) % MAX_EP93XX_DMA_BUFFERS].last) ||
		    (dma->new_buffers == 0) || (dma->xfer_enable == FALSE)) {
			DPRINTK("7  ");

			/*
			 * This is the last Buffer in this transaction, so
			 * disable the NFB interrupt.  We shouldn't get an NFB
			 * int when the FSM moves to the ON state where it
			 * would typically get the NFB int indicating a new
			 * buffer can be programmed.  Instead, once in the ON
			 * state, the DMA will just proceed to complete the
			 * transfer of the current buffer, move the FSB
			 * directly to the STALL state where a STALL interrupt
			 * will be generated.
			 */
			uiCONTROL = inl(M2M_reg_base+M2M_OFFSET_CONTROL);
			uiCONTROL &= ~CONTROL_M2M_NFBINTEN ;
			outl( uiCONTROL, M2M_reg_base+M2M_OFFSET_CONTROL );

			/*
			 * The current buffer has been transferred, so
			 * increment the current buffer counter to reflect
			 * this.
			 */
			dma->current_buffer = (dma->current_buffer + 1) %
					      MAX_EP93XX_DMA_BUFFERS;

			DPRINTK("End of NFB handling. \n");
			DPRINTK("CONTROL - 0x%x \n",
                                inl(M2M_reg_base+M2M_OFFSET_CONTROL) );
			DPRINTK("STATUS - 0x%x \n",
                                inl(M2M_reg_base+M2M_OFFSET_STATUS) );
			DPRINTK("SAR_BASE0 - 0x%x \n",
                                inl(M2M_reg_base+M2M_OFFSET_SAR_BASE0) );
			DPRINTK("SAR_CUR0 - 0x%x \n",
                                inl(M2M_reg_base+M2M_OFFSET_SAR_CURRENT0) );
			DPRINTK("DAR_BASE0 - 0x%x \n",
                                inl(M2M_reg_base+M2M_OFFSET_DAR_BASE0) );
			DPRINTK("DAR_CUR0 - 0x%x \n",
                                inl(M2M_reg_base+M2M_OFFSET_DAR_CURRENT0) );

			DPRINTK("Buffer	buf_id	 source	   size	   last	   used \n");
			for (loop = 0; loop < 32; loop ++)
				DPRINTK("%d		0x%x		0x%x		 0x%x		%d		 %d \n",
					loop, dma->buffer_queue[loop].buf_id,
					dma->buffer_queue[loop].source,
					dma->buffer_queue[loop].size,
					dma->buffer_queue[loop].last,
					dma->buffer_queue[loop].used);
			DPRINTK("pause	 0x%x		0x%x		 0x%x		%d		 %d \n",
				dma->pause_buf.buf_id, dma->pause_buf.source,
				dma->pause_buf.size, dma->pause_buf.last,
				dma->pause_buf.used);

			DPRINTK("Pause - %d \n", dma->pause);
			DPRINTK("xfer_enable - %d \n", dma->xfer_enable);
			DPRINTK("total bytes - 0x%x \n", dma->total_bytes);
			DPRINTK("total buffer - %d \n", dma->total_buffers);
			DPRINTK("new buffers - %d \n", dma->new_buffers);
			DPRINTK("current buffer - %d \n", dma->current_buffer);
			DPRINTK("last buffer - %d \n", dma->last_buffer);
			DPRINTK("used buffers - %d \n", dma->used_buffers);
			DPRINTK("callback addr - 0x%p \n", dma->callback);

		} else if (dma->new_buffers) {
			DPRINTK("8  ");
			/*
			 * We have a new buffer, so increment the current
			 * buffer to point to the next buffer, which is already
			 * programmed into the DMA. Next time around, it'll be
			 * pointing to the current buffer.
			 */
			dma->current_buffer = (dma->current_buffer + 1) %
					      MAX_EP93XX_DMA_BUFFERS;

			/*
			 * We know we have a new buffer to program as the next
			 * buffer, so check which set of SAR_BASE/DAR_BASE/BCR
			 * registers to program.
			 */
			if ( inl(M2M_reg_base+M2M_OFFSET_STATUS) & STATUS_M2M_NB ) {
				/*
				 * Set the SAR_BASE1/DAR_BASE1/BCR1 registers
				 * with values from the next buffer in the
				 * queue.
				 */
				outl( dma->buffer_queue[(dma->current_buffer + 1) %
							MAX_EP93XX_DMA_BUFFERS].source,
				      M2M_reg_base+M2M_OFFSET_SAR_BASE1 );

				outl( dma->buffer_queue[(dma->current_buffer + 1) %
							MAX_EP93XX_DMA_BUFFERS].dest,
				      M2M_reg_base+M2M_OFFSET_DAR_BASE1 );

				outl( dma->buffer_queue[(dma->current_buffer + 1) %
							MAX_EP93XX_DMA_BUFFERS].size,
				      M2M_reg_base+M2M_OFFSET_BCR1 );
			} else {
				/*
				 * Set the SAR_BASE0/DAR_BASE0/BCR0 registers
				 * with values from the next buffer in the
				 * queue.
				 */
				outl( dma->buffer_queue[(dma->current_buffer + 1) %
							MAX_EP93XX_DMA_BUFFERS].source,
				      M2M_reg_base+M2M_OFFSET_SAR_BASE0 );

				outl( dma->buffer_queue[(dma->current_buffer + 1) %
							MAX_EP93XX_DMA_BUFFERS].dest,
				      M2M_reg_base+M2M_OFFSET_DAR_BASE0 );

				outl( dma->buffer_queue[(dma->current_buffer + 1) %
							MAX_EP93XX_DMA_BUFFERS].size,
				      M2M_reg_base+M2M_OFFSET_BCR0 );
			}

			/*
			 *  Decrement the new buffers counter
			 */
			dma->new_buffers--;
		}
	} else {
		/*
		 * Total number of buffers is 0 - really we should never get
		 * here, but just in case.
		 */
		DPRINTK("9 \n");

		/*
		 *  No new buffers to transfer, so Disable the channel
		 */
		uiCONTROL = inl(M2M_reg_base+M2M_OFFSET_CONTROL);
		uiCONTROL &= ~CONTROL_M2M_ENABLE;
		outl( uiCONTROL, M2M_reg_base+M2M_OFFSET_CONTROL );

		/*
		 *  Indicate that the channel is paused by starvation.
		 */
		dma->pause = 1;
	}
}

/*****************************************************************************
 *
 * dma_m2m_irq_handler
 *
 ****************************************************************************/
static void
dma_m2m_irq_handler(int irq, void *dev_id, struct pt_regs *regs)
{
	ep93xx_dma_t *dma = (ep93xx_dma_t *)dev_id;
	unsigned int M2M_reg_base = dma->reg_base;
	ep93xx_dma_dev_t dma_int = UNDEF_INT;
	int status;

//	printk("+m2m irq=%d\n", irq);

	/*
	 *  Determine what kind of dma interrupt this is.
	 */
	status = inl(M2M_reg_base + M2M_OFFSET_INTERRUPT);
	if ( status & INTERRUPT_M2M_DONEINT )
		dma_int = DONE; // we're done with a requested dma
	else if ( status & INTERRUPT_M2M_NFBINT )
		dma_int = NFB;  // we're done with one dma buffer

	DPRINTK("IRQ: b=%#x st=%#x\n", (int)dma->current_buffer, dma_int);

	switch (dma_int) {
	/*
	 *  Next Frame Buffer Interrupt.  If there's a new buffer program it
	 *  Check if this is the last buffer in the transfer,
	 *  and if it is, disable the NFB int to prevent being
	 *  interrupted for another buffer when we know there won't be
	 *  another.
	 */
	case NFB:
		dma_m2m_next_frame_buffer(dma);
		break;
	/*
	 *  Done interrupt generated, indicating that the transfer is complete.
	 */
	case DONE:
		dma_m2m_transfer_done(dma);
		break;

	default:
		break;
	}

	if ((dma_int != UNDEF_INT) && dma->callback)
		dma->callback(dma_int, dma->device, dma->user_data);
}

/*****************************************************************************
 *
 * dma_m2p_irq_handler
 *
 *
 *
 ****************************************************************************/
static void
dma_m2p_irq_handler(int irq, void *dev_id, struct pt_regs *regs)
{
	ep93xx_dma_t *dma = (ep93xx_dma_t *) dev_id;
	unsigned int M2P_reg_base = dma->reg_base;
	unsigned int read_back;
	ep93xx_dma_dev_t dma_int = UNDEF_INT;
	unsigned int loop, uiCONTROL, uiINTERRUPT;

	/*
	 *  Determine what kind of dma interrupt this is.
	 */
	if ( inl(M2P_reg_base+M2P_OFFSET_INTERRUPT) & INTERRUPT_M2P_STALLINT )
		dma_int = STALL;
	else if ( inl(M2P_reg_base+M2P_OFFSET_INTERRUPT) & INTERRUPT_M2P_NFBINT )
		dma_int = NFB;
	else if ( inl(M2P_reg_base+M2P_OFFSET_INTERRUPT) & INTERRUPT_M2P_CHERRORINT )
		dma_int = CHERROR;

	/*
	 *  Stall Interrupt: The Channel is stalled, meaning nothing is
	 *  programmed to transfer right now.  So, we're back to the
	 *  beginnning.  If there's a buffer to transfer, program it into
	 *  max and base 0 registers.
	 */
	if (dma_int == STALL) {
		DPRINTK("1  ");

		if (dma->total_buffers) {
			/*
			 * The current_buffer has already been tranfered, so
			 * add the byte count to the total_bytes field.
			 */
			dma->total_bytes = dma->total_bytes +
				dma->buffer_queue[dma->current_buffer].size;

			/*
			 *  Mark the current_buffer as used.
			 */
			dma->buffer_queue[dma->current_buffer].used = TRUE;

			/*
			 *  Increment the used buffer counter
			 */
			dma->used_buffers++;

			DPRINTK("#%d", dma->current_buffer);

			/*
			 *  Increment the current_buffer
			 */
			dma->current_buffer = (dma->current_buffer + 1) %
					      MAX_EP93XX_DMA_BUFFERS;

			/*
			 *  check if there's a new buffer to transfer.
			 */
			if (dma->new_buffers && dma->xfer_enable) {
				/*
				 * We have a new buffer to transfer so program
				 * in the buffer values.  Since a STALL
				 * interrupt was triggered, we program the
				 * base0 and maxcnt0
				 *
				 * Set the MAXCNT0 register with the buffer
				 * size
				 */
				outl( dma->buffer_queue[dma->current_buffer].size,
					  M2P_reg_base+M2P_OFFSET_MAXCNT0 );

				/*
				 * Set the BASE0 register with the buffer base
				 * address
				 */
				outl( dma->buffer_queue[dma->current_buffer].source,
					  M2P_reg_base+M2P_OFFSET_BASE0 );

				/*
				 *  Decrement the new buffer counter
				 */
				dma->new_buffers--;

				if (dma->new_buffers) {
					DPRINTK("A  ");
					/*
					 * Set the MAXCNT1 register with the
					 * buffer size
					 */
					outl( dma->buffer_queue[(dma->current_buffer + 1) %
											MAX_EP93XX_DMA_BUFFERS].size,
						  M2P_reg_base+M2P_OFFSET_MAXCNT1 );

					/*
					 * Set the BASE1 register with the
					 * buffer base address
					 */
					outl( dma->buffer_queue[dma->current_buffer + 1 %
											MAX_EP93XX_DMA_BUFFERS].source,
						  M2P_reg_base+M2P_OFFSET_BASE1 );

					/*
					 *  Decrement the new buffer counter
					 */
					dma->new_buffers--;

					/*
					 *  Enable the NFB Interrupt.
					 */
					uiCONTROL = inl(M2P_reg_base+M2P_OFFSET_CONTROL);
					uiCONTROL |= CONTROL_M2P_NFBINTEN;
					outl( uiCONTROL, M2P_reg_base+M2P_OFFSET_CONTROL );
				}
			} else {
				/*
				 *  No new buffers.
				 */
				DPRINTK("2 \n");

				/*
				 *  There's a chance we setup both buffer descriptors, but
				 *  didn't service the NFB quickly enough, causing the channel
				 *  to transfer both buffers, then enter the stall state.
				 *  So, we need to be able to process the second buffer.
				 */
				if ((dma->used_buffers + dma->new_buffers) < dma->total_buffers) {
					DPRINTK("3 ");

					/*
					 *  The current_buffer has already been tranfered, so add the
					 *  byte count to the total_bytes field.
					 */
					dma->total_bytes = dma->total_bytes +
						dma->buffer_queue[dma->current_buffer].size;

					/*
					 *  Mark the current_buffer as used.
					 */
					dma->buffer_queue[dma->current_buffer].used = TRUE;

					/*
					 *  Increment the used buffer counter
					 */
					dma->used_buffers++;

					DPRINTK("#%d", dma->current_buffer);

					/*
					 *  Increment the current buffer pointer.
					 */
					dma->current_buffer = (dma->current_buffer + 1) %
						MAX_EP93XX_DMA_BUFFERS;

				}

				/*
				 *  No new buffers to transfer, so disable the channel.
				 */
				uiCONTROL = inl(M2P_reg_base+M2P_OFFSET_CONTROL);
				uiCONTROL &= ~CONTROL_M2P_ENABLE;
				outl( uiCONTROL, M2P_reg_base+M2P_OFFSET_CONTROL );

				/*
				 *  Indicate that this channel is in the pause by starvation
				 *  state by setting the pause bit to true.
				 */
				dma->pause = TRUE;

				DPRINTK("STATUS - 0x%x \n",	 inl(M2P_reg_base+M2P_OFFSET_STATUS) );
				DPRINTK("CONTROL - 0x%x \n",	inl(M2P_reg_base+M2P_OFFSET_CONTROL) );
				DPRINTK("REMAIN - 0x%x \n",	 inl(M2P_reg_base+M2P_OFFSET_REMAIN) );
				DPRINTK("PPALLOC - 0x%x \n",	inl(M2P_reg_base+M2P_OFFSET_PPALLOC) );
				DPRINTK("BASE0 - 0x%x \n",	  inl(M2P_reg_base+M2P_OFFSET_BASE0) );
				DPRINTK("MAXCNT0 - 0x%x \n",	inl(M2P_reg_base+M2P_OFFSET_MAXCNT0) );
				DPRINTK("CURRENT0 - 0x%x \n",   inl(M2P_reg_base+M2P_OFFSET_CURRENT0) );
				DPRINTK("BASE1 - 0x%x \n",	  inl(M2P_reg_base+M2P_OFFSET_BASE1) );
				DPRINTK("MAXCNT1 - 0x%x \n",	inl(M2P_reg_base+M2P_OFFSET_MAXCNT1) );
				DPRINTK("CURRENT1 - 0x%x \n",   inl(M2P_reg_base+M2P_OFFSET_CURRENT1) );

				DPRINTK("Buffer	buf_id	 source	   size	   last	   used \n");
				for (loop = 0; loop < 32; loop ++)
					DPRINTK("%d		0x%x		0x%x		 0x%x		%d		 %d \n",
							loop, dma->buffer_queue[loop].buf_id, dma->buffer_queue[loop].source,
							dma->buffer_queue[loop].size,
							dma->buffer_queue[loop].last, dma->buffer_queue[loop].used);
				DPRINTK("pause	 0x%x		0x%x		 0x%x		%d		 %d \n",
						dma->pause_buf.buf_id, dma->pause_buf.source, dma->pause_buf.size,
						dma->pause_buf.last, dma->pause_buf.used);

				DPRINTK("Pause - %d \n", dma->pause);
				DPRINTK("xfer_enable - %d \n", dma->xfer_enable);
				DPRINTK("total bytes - 0x%x \n", dma->total_bytes);
				DPRINTK("total buffer - %d \n", dma->total_buffers);
				DPRINTK("new buffers - %d \n", dma->new_buffers);
				DPRINTK("current buffer - %d \n", dma->current_buffer);
				DPRINTK("last buffer - %d \n", dma->last_buffer);
				DPRINTK("used buffers - %d \n", dma->used_buffers);
				DPRINTK("callback addr - 0x%p \n", dma->callback);
			}
		} else {
			/*
			 *  No buffers to transfer, or old buffers to mark as used,
			 *  so Disable the channel
			 */
			uiCONTROL = inl(M2P_reg_base+M2P_OFFSET_CONTROL);
			uiCONTROL &= ~CONTROL_M2P_ENABLE;
			outl( uiCONTROL, M2P_reg_base+M2P_OFFSET_CONTROL );

			/*
			 *  Must read the control register back after a write.
			 */
			read_back = inl(M2P_reg_base+M2P_OFFSET_CONTROL);

			/*
			 *  Indicate that this channel is in the pause by
			 *  starvation state by setting the pause bit to true.
			 */
			dma->pause = TRUE;
		}
	}

	/*
	 *  Next Frame Buffer Interrupt.  If there's a new buffer program it
	 *  Check if this is the last buffer in the transfer,
	 *  and if it is, disable the NFB int to prevent being
	 *  interrupted for another buffer when we know there won't be
	 *  another.
	 */
	if (dma_int == NFB) {
		DPRINTK("5  ");

		if (dma->total_buffers) {
			DPRINTK("6  ");
			/*
			 *  The iCurrentBuffer has already been transfered.  so add the
			 *  byte count from the current buffer to the total byte count.
			 */
			dma->total_bytes = dma->total_bytes +
				dma->buffer_queue[dma->current_buffer].size;

			/*
			 *  Mark the Current Buffer as used.
			 */
			dma->buffer_queue[dma->current_buffer].used = TRUE;

			/*
			 *  Increment the used buffer counter
			 */
			dma->used_buffers++;

			DPRINTK("#%d", dma->current_buffer);

			if ((dma->buffer_queue[
			    (dma->current_buffer + 1) % MAX_EP93XX_DMA_BUFFERS].last) ||
			    (dma->new_buffers == 0) || (dma->xfer_enable == FALSE)) {
				DPRINTK("7  ");

				/*
				 *  This is the last Buffer in this transaction, so disable
				 *  the NFB interrupt.  We shouldn't get an NFB int when the
				 *  FSM moves to the ON state where it would typically get the
				 *  NFB int indicating a new buffer can be programmed.
				 *  Instead, once in the ON state, the DMA will just proceed
				 *  to complet the transfer of the current buffer, move the
				 *  FSB directly to the STALL state where a STALL interrupt
				 *  will be generated.
				 */
				uiCONTROL = inl(M2P_reg_base+M2P_OFFSET_CONTROL);
				uiCONTROL &= ~CONTROL_M2P_NFBINTEN;
				outl( uiCONTROL, M2P_reg_base+M2P_OFFSET_CONTROL );

				/*
				 *  The current buffer has been transferred, so increment
				 *  the current buffer counter to reflect this.
				 */
				dma->current_buffer = (dma->current_buffer + 1) % MAX_EP93XX_DMA_BUFFERS;

				DPRINTK("End of NFB handling. \n");
				DPRINTK("STATUS - 0x%x \n",	 inl(M2P_reg_base+M2P_OFFSET_STATUS) );
				DPRINTK("CONTROL - 0x%x \n",	inl(M2P_reg_base+M2P_OFFSET_CONTROL) );
				DPRINTK("REMAIN - 0x%x \n",	 inl(M2P_reg_base+M2P_OFFSET_REMAIN) );
				DPRINTK("PPALLOC - 0x%x \n",	inl(M2P_reg_base+M2P_OFFSET_PPALLOC) );
				DPRINTK("BASE0 - 0x%x \n",	  inl(M2P_reg_base+M2P_OFFSET_BASE0) );
				DPRINTK("MAXCNT0 - 0x%x \n",	inl(M2P_reg_base+M2P_OFFSET_MAXCNT0) );
				DPRINTK("CURRENT0 - 0x%x \n",   inl(M2P_reg_base+M2P_OFFSET_CURRENT0) );
				DPRINTK("BASE1 - 0x%x \n",	  inl(M2P_reg_base+M2P_OFFSET_BASE1) );
				DPRINTK("MAXCNT1 - 0x%x \n",	inl(M2P_reg_base+M2P_OFFSET_MAXCNT1) );
				DPRINTK("CURRENT1 - 0x%x \n",   inl(M2P_reg_base+M2P_OFFSET_CURRENT1) );

				DPRINTK("Buffer	buf_id	 source	   size	   last	   used \n");
				for (loop = 0; loop < 32; loop ++)
					DPRINTK("%d		0x%x		0x%x		 0x%x		%d		 %d \n",
							loop, dma->buffer_queue[loop].buf_id, dma->buffer_queue[loop].source,
							dma->buffer_queue[loop].size,
							dma->buffer_queue[loop].last, dma->buffer_queue[loop].used);
				DPRINTK("pause	 0x%x		0x%x		 0x%x		%d		 %d \n",
						dma->pause_buf.buf_id, dma->pause_buf.source, dma->pause_buf.size,
						dma->pause_buf.last, dma->pause_buf.used);

				DPRINTK("Pause - %d \n", dma->pause);
				DPRINTK("xfer_enable - %d \n", dma->xfer_enable);
				DPRINTK("total bytes - 0x%x \n", dma->total_bytes);
				DPRINTK("total buffer - %d \n", dma->total_buffers);
				DPRINTK("new buffers - %d \n", dma->new_buffers);
				DPRINTK("current buffer - %d \n", dma->current_buffer);
				DPRINTK("last buffer - %d \n", dma->last_buffer);
				DPRINTK("used buffers - %d \n", dma->used_buffers);
				DPRINTK("callback addr - 0x%p \n", dma->callback);

			} else if (dma->new_buffers) {
				DPRINTK("8  ");
				/*
				 *  we have a new buffer, so increment the current buffer to
				 *  point to the next buffer, which is already programmed into
				 *  the DMA. Next time around, it'll be pointing to the
				 *  current buffer.
				 */
				dma->current_buffer = (dma->current_buffer + 1) % MAX_EP93XX_DMA_BUFFERS;

				/*
				 *  we know we have a new buffer to program as the next
				 *  buffer, so check which set of MAXCNT and BASE registers
				 *  to program.
				 */
				if ( inl(M2P_reg_base+M2P_OFFSET_STATUS) & STATUS_M2P_NEXTBUFFER ) {
					/*
					 *  Set the MAXCNT1 register with the buffer size
					 */
					outl( dma->buffer_queue[
					      (dma->current_buffer + 1) % MAX_EP93XX_DMA_BUFFERS].size,
					      M2P_reg_base+M2P_OFFSET_MAXCNT1 );

					/*
					 *  Set the BASE1 register with the buffer base address
					 */
					outl( dma->buffer_queue[
					      (dma->current_buffer + 1) % MAX_EP93XX_DMA_BUFFERS].source,
					      M2P_reg_base+M2P_OFFSET_BASE1 );
				} else {
					/*
					 *  Set the MAXCNT0 register with the buffer size
					 */
					outl( dma->buffer_queue[
					      (dma->current_buffer + 1) % MAX_EP93XX_DMA_BUFFERS].size,
					       M2P_reg_base+M2P_OFFSET_MAXCNT0 );

					/*
					 *  Set the BASE0 register with the buffer base address
					 */
					outl( dma->buffer_queue[
					      (dma->current_buffer + 1) % MAX_EP93XX_DMA_BUFFERS].source,
					      M2P_reg_base+M2P_OFFSET_BASE0 );
				}

				/*
				 *  Decrement the new buffers counter
				 */
				dma->new_buffers--;
			}
		} else {
			/*
			 *  Total number of buffers is 0 - really we should never get here,
			 *  but just in case.
			 */
			DPRINTK("9 \n");

			/*
			 *  No new buffers to transfer, so Disable the channel
			 */
			uiCONTROL = inl(M2P_reg_base+M2P_OFFSET_CONTROL);
			uiCONTROL &= ~CONTROL_M2P_ENABLE;
			outl( uiCONTROL, M2P_reg_base+M2P_OFFSET_CONTROL );
		}
	}

	/*
	 *  Channel Error Interrupt, or perhipheral interrupt, specific to the
	 *  memory to/from peripheral channels.
	 */
	if (dma_int == CHERROR) {
		/*
		 *  just clear the interrupt, it's really up to the peripheral
		 *  driver to determine if any further action is necessary.
		 */
		uiINTERRUPT = inl(M2P_reg_base+M2P_OFFSET_INTERRUPT);
		uiINTERRUPT &= ~INTERRUPT_M2P_CHERRORINT;
		outl( uiINTERRUPT, M2P_reg_base+M2P_OFFSET_INTERRUPT );
	}

	/*
	 *  Make sure the interrupt was valid, and if it was, then check
	 *  if a callback function was installed for this DMA channel.  If a
	 *  callback was installed call it.
	 */
	if ((dma_int != UNDEF_INT) && dma->callback)
		dma->callback(dma_int, dma->device, dma->user_data);
}

/*****************************************************************************
 *
 * ep9312_dma_open_m2p(int device)
 *
 * Description: This function will attempt to open a M2P/P2M DMA channel.
 *			  If the open is successful, the channel number is returned,
 *			  otherwise a negative number is returned.
 *
 * Parameters:
 *  device:	 device for which the dma channel is requested.
 *
 ****************************************************************************/
static int
dma_open_m2p(int device)
{
	int channel = -1;
	unsigned int loop;
	unsigned int M2P_reg_base;
	unsigned int uiPWRCNT;
	unsigned long flags;

	DPRINTK("DMA Open M2P with hw dev %d\n", device);

	/*
	 *  Lock the dma channel list.
	 */
	spin_lock_irqsave(&dma_list_lock, flags);

	DPRINTK("1\n");
	/*
	 * Verify that the device requesting DMA isn't already using a DMA channel
	 */
	if (device >= 10)
		loop = 1;		 // Rx transfer requested
	else
		loop = 0;		 // Tx transfer requested

	for (; loop < 10; loop = loop + 2)
		/*
		 *  Before checking for a matching device, check that the
		 *  channel is in use, otherwise the device field is
		 *  invalid.
		 */
		if (dma_chan[loop].ref_count)
			if (device == dma_chan[loop].device) {
				DPRINTK("DMA Open M2P - Error\n");
				return(-1);
			}

	/*
	 *  Get a DMA channel instance for the given hardware device.
	 *  If this is a TX look for even numbered channels, else look for
	 *  odd numbered channels
	 */
	if (device >= 10)
		loop = 1;		 /* Rx transfer requested */
	else
		loop = 0;		 /* Tx transfer requested */

	for (; loop < 10; loop = loop + 2)
		if (!dma_chan[loop].ref_count) {
			/*
			 *  Capture the channel and increment the reference count.
			 */
			channel = loop;
			dma_chan[channel].ref_count++;
			break;
		}

	/*
	 *  Unlock the dma channel list.
	 */
	spin_unlock_irqrestore(&dma_list_lock, flags);

	/*
	 *  See if we got a valid channel.
	 */
	if (channel < 0)
		return(-1);

	/*
	 *  Point regs to the correct dma channel register base.
	 */
	M2P_reg_base = dma_chan[channel].reg_base;

	/*
	 *  Turn on the clock for the specified DMA channel
	 *  TODO: need to use the correct register name for the
	 *  power control register.
	 */
	uiPWRCNT = inl(SYSCON_PWRCNT);
	switch (channel) {
	case 0:
		uiPWRCNT |= SYSCON_PWRCNT_DMA_M2PCH0;
		break;

	case 1:
		uiPWRCNT |= SYSCON_PWRCNT_DMA_M2PCH1;
		break;

	case 2:
		uiPWRCNT |= SYSCON_PWRCNT_DMA_M2PCH2;
		break;

	case 3:
		uiPWRCNT |= SYSCON_PWRCNT_DMA_M2PCH3;
		break;

	case 4:
		uiPWRCNT |= SYSCON_PWRCNT_DMA_M2PCH4;
		break;

	case 5:
		uiPWRCNT |= SYSCON_PWRCNT_DMA_M2PCH5;
		break;

	case 6:
		uiPWRCNT |= SYSCON_PWRCNT_DMA_M2PCH6;
		break;

	case 7:
		uiPWRCNT |= SYSCON_PWRCNT_DMA_M2PCH7;
		break;

	case 8:
		uiPWRCNT |= SYSCON_PWRCNT_DMA_M2PCH8;
		break;

	case 9:
		uiPWRCNT |= SYSCON_PWRCNT_DMA_M2PCH9;
		break;

	default:
		return(-1);
	}
	outl( uiPWRCNT, SYSCON_PWRCNT );

	/*
	 *  Clear out the control register before any further setup.
	 */
	outl( 0, M2P_reg_base+M2P_OFFSET_CONTROL );

	/*
	 *  Setup the peripheral port value in the DMA channel registers.
	 */
	if (device < 10)
		outl( (unsigned int)device, M2P_reg_base+M2P_OFFSET_PPALLOC );
	else
		outl( (unsigned int)(device - 10), M2P_reg_base+M2P_OFFSET_PPALLOC );

	/*
	 *  Let's hold on to the value of the Hw device for comparison later.
	 */
	dma_chan[channel].device = device;

	/*
	 *  Success.
	 */
	return(channel);
}

/*****************************************************************************
 *
 * dma_open_m2m(int device)
 *
 * Description: This function will attempt to open a M2M DMA channel.
 *			  If the open is successful, the channel number is returned,
 *			  otherwise a negative number is returned.
 *
 * Parameters:
 *  device:	 device for which the dma channel is requested.
 *
 ****************************************************************************/
static int
dma_open_m2m(int device)
{
	int channel = -1;
	unsigned int loop;
	unsigned int M2M_reg_base;
	unsigned int uiPWRCNT, uiCONTROL;
	unsigned long flags;

	DPRINTK("DMA Open M2M with hw dev %d\n", device);

	/*
	 *  Lock the dma channel list.
	 */
	spin_lock_irqsave(&dma_list_lock, flags);

	DPRINTK("1\n");

	/*
	 *  Check if this device is already allocated a channel.
	 *  TODO: can one M2M device be allocated multiple channels?
	 */
	for (loop = DMA_MEMORY; loop < UNDEF; loop++)
		/*
		 *  Before checking for a matching device, check that the
		 *  channel is in use, otherwise the device field is
		 *  invalid.
		 */
		if (dma_chan[loop].ref_count)
			if (device == dma_chan[loop].device) {
				DPRINTK("Error - dma_open_m2m - already allocated channel\n");

				/*
				 *  Unlock the dma channel list.
				 */
				spin_unlock_irqrestore(&dma_list_lock, flags);

				/*
				 *  Fail.
				 */
				return(-1);
			}

	/*
	 *  Get a DMA channel instance for the given hardware device.
	 */
	for (loop = 10; loop < 12; loop++)
		if (!dma_chan[loop].ref_count) {
			/*
			 *  Capture the channel and increment the reference count.
			 */
			channel = loop;
			dma_chan[channel].ref_count++;
			break;
		}

	/*
	 *  Unlock the dma channel list.
	 */
	spin_unlock(dma_list_lock);

	/*
	 *  See if we got a valid channel.
	 */
	if (channel < 0)
		return(-1);

	/*
	 *  Point regs to the correct dma channel register base.
	 */
	M2M_reg_base = dma_chan[channel].reg_base;

	/*
	 *  Turn on the clock for the specified DMA channel
	 *  TODO: need to use the correct register name for the
	 *  power control register.
	 */
	uiPWRCNT = inl(SYSCON_PWRCNT);
	switch (channel) {
	case 10:
		uiPWRCNT |= SYSCON_PWRCNT_DMA_M2MCH0;
		break;

	case 11:
		uiPWRCNT |= SYSCON_PWRCNT_DMA_M2MCH1;
		break;

	default:
		return(-1);
	}
	outl( uiPWRCNT, SYSCON_PWRCNT );

	DPRINTK("DMA Open - power control: 0x%x \n", inl(SYSCON_PWRCNT) );

	/*
	 *  Clear out the control register before any further setup.
	 */
	outl( 0, M2M_reg_base+M2M_OFFSET_CONTROL );

	/*
	 *  Setup the transfer mode and the request source selection within
	 *  the DMA M2M channel registers.
	 */
	switch (device) {
	case DMA_MEMORY:
		/*
		 * Clear TM field, set RSS field to 0
		 */
		uiCONTROL = inl(M2M_reg_base+M2M_OFFSET_CONTROL);
		uiCONTROL &= ~(CONTROL_M2M_TM_MASK | CONTROL_M2M_RSS_MASK);
		outl( uiCONTROL, M2M_reg_base+M2M_OFFSET_CONTROL );
		break;

	case DMA_IDE:
		/*
		 * Set RSS field to 3, Set NO_HDSK, Set PW field to 1
		 */
		uiCONTROL = inl(M2M_reg_base+M2M_OFFSET_CONTROL);
		uiCONTROL &= ~(CONTROL_M2M_RSS_MASK|CONTROL_M2M_PW_MASK);
		uiCONTROL |= (3<<CONTROL_M2M_RSS_SHIFT) |
			CONTROL_M2M_NO_HDSK |
			(2<<CONTROL_M2M_PW_SHIFT);

		uiCONTROL &= ~(CONTROL_M2M_ETDP_MASK);
		uiCONTROL &= ~(CONTROL_M2M_DACKP);
		uiCONTROL &= ~(CONTROL_M2M_DREQP_MASK);

		outl( uiCONTROL, M2M_reg_base+M2M_OFFSET_CONTROL );
		inl(M2M_reg_base+M2M_OFFSET_CONTROL);
		break;

	case DMARx_SSP:
		/*
		 * Set RSS field to 1, Set NO_HDSK, Set TM field to 2
		 */
		uiCONTROL = inl(M2M_reg_base+M2M_OFFSET_CONTROL);
		uiCONTROL &= ~(CONTROL_M2M_RSS_MASK|CONTROL_M2M_TM_MASK);
		uiCONTROL |= (1<<CONTROL_M2M_RSS_SHIFT) |
			CONTROL_M2M_NO_HDSK |
			(2<<CONTROL_M2M_TM_SHIFT);
		outl( uiCONTROL, M2M_reg_base+M2M_OFFSET_CONTROL );
		break;

	case DMATx_SSP:
		/*
		 * Set RSS field to 2, Set NO_HDSK, Set TM field to 1
		 */
		uiCONTROL = inl(M2M_reg_base+M2M_OFFSET_CONTROL);
		uiCONTROL &= ~(CONTROL_M2M_RSS_MASK|CONTROL_M2M_TM_MASK);
		uiCONTROL |= (2<<CONTROL_M2M_RSS_SHIFT) |
			CONTROL_M2M_NO_HDSK |
			(1<<CONTROL_M2M_TM_SHIFT);
		outl( uiCONTROL, M2M_reg_base+M2M_OFFSET_CONTROL );
		break;

	case DMATx_EXT_DREQ:
		/*
		 * Set TM field to 2, set RSS field to 0
		 */
		uiCONTROL = inl(M2M_reg_base+M2M_OFFSET_CONTROL);
		uiCONTROL &= ~(CONTROL_M2M_RSS_MASK|CONTROL_M2M_TM_MASK);
		uiCONTROL |= 1<<CONTROL_M2M_TM_SHIFT;
		outl( uiCONTROL, M2M_reg_base+M2M_OFFSET_CONTROL );
		break;

	case DMARx_EXT_DREQ:
		/*
		 * Set TM field to 2, set RSS field to 0
		 */
		uiCONTROL = inl(M2M_reg_base+M2M_OFFSET_CONTROL);
		uiCONTROL &= ~(CONTROL_M2M_RSS_MASK|CONTROL_M2M_TM_MASK);
		uiCONTROL |= 2<<CONTROL_M2M_TM_SHIFT;
		outl( uiCONTROL, M2M_reg_base+M2M_OFFSET_CONTROL );
		break;

	default:
		return -1;
	}

	/*
	 *  Let's hold on to the value of the Hw device for comparison later.
	 */
	dma_chan[channel].device = device;

	/*
	 *  Success.
	 */
	return(channel);
}

/*****************************************************************************
 *
 *  int dma_config_m2m(ep93xx_dma_t * dma, unsigned int flags_m2m,
 *			   dma_callback callback, unsigned int user_data)
 *
 *  Description: Configure the DMA channel and install a callback function.
 *			   This function will have to be called for every transfer
 *
 *  dma:		Pointer to the dma instance data for the M2M channel to
 *			  configure.
 *  flags_m2m   Flags used to configure an M2M dma channel and determine
 *			  if a callback function and user_data information are included
 *			  in this call.
 *  callback	function pointer which is called near the end of the
 *			  dma channel's irq handler.
 *  user_data   defined by the calling driver.
 *
 ****************************************************************************/
static int
dma_config_m2m(ep93xx_dma_t * dma, unsigned int flags_m2m,
			   dma_callback callback, unsigned int user_data)
{
	unsigned int flags;
	unsigned int M2M_reg_base, uiCONTROL;

	/*
	 *  Make sure the channel is disabled before configuring the channel.
	 *
	 *  TODO: Is this correct??   Making a big change here...
	 */
	/* if (!dma->pause || (!dma->pause && dma->xfer_enable)) */
	if (dma->xfer_enable) {
		/*
		 *  DMA channel is not paused, so we can't configure it.
		 */
		DPRINTK("DMA channel not paused, so can't configure! \n");
		return(-1);
	}

	/*
	 *  Mask interrupts.
	 */
	local_irq_save(flags);

	/*
	 *  Setup a pointer into the dma channel's register set.
	 */
	M2M_reg_base = dma->reg_base;

	uiCONTROL = inl(M2M_reg_base + M2M_OFFSET_CONTROL);
	outl(0, M2M_reg_base + M2M_OFFSET_CONTROL);
	inl(M2M_reg_base + M2M_OFFSET_CONTROL);
	outl(uiCONTROL, M2M_reg_base + M2M_OFFSET_CONTROL);

	/*
	 *  By default we disable the stall interrupt.
	 */
	uiCONTROL = inl(M2M_reg_base+M2M_OFFSET_CONTROL);
	uiCONTROL &= ~CONTROL_M2M_STALLINTEN;
	outl( uiCONTROL, M2M_reg_base+M2M_OFFSET_CONTROL );

	/*
	 *  By default we disable the done interrupt.
	 */
	uiCONTROL = inl(M2M_reg_base+M2M_OFFSET_CONTROL);
	uiCONTROL &= ~CONTROL_M2M_DONEINTEN;
	outl( uiCONTROL, M2M_reg_base+M2M_OFFSET_CONTROL );

	/*
	 *  Set up the transfer control fields based on values passed in
	 *  the flags_m2m field.
	 */
	uiCONTROL = inl(M2M_reg_base+M2M_OFFSET_CONTROL);

	if ( flags_m2m & DESTINATION_HOLD )
		uiCONTROL |= CONTROL_M2M_DAH;
	else
		uiCONTROL &= ~CONTROL_M2M_DAH;

	if ( flags_m2m & SOURCE_HOLD )
		uiCONTROL |= CONTROL_M2M_SAH;
	else
		uiCONTROL &= ~CONTROL_M2M_SAH;

	uiCONTROL &= ~CONTROL_M2M_TM_MASK;
	uiCONTROL |= (((flags_m2m & TRANSFER_MODE_MASK) >> TRANSFER_MODE_SHIFT) <<
				  CONTROL_M2M_TM_SHIFT) & CONTROL_M2M_TM_MASK;

	uiCONTROL &= ~CONTROL_M2M_PWSC_MASK;
	uiCONTROL |= (((flags_m2m & WAIT_STATES_MASK) >> WAIT_STATES_SHIFT) <<
				  CONTROL_M2M_PWSC_SHIFT) & CONTROL_M2M_PWSC_MASK;

	outl( uiCONTROL, M2M_reg_base+M2M_OFFSET_CONTROL );
	inl(M2M_reg_base + M2M_OFFSET_CONTROL);

	/*
	 *  Save the callback function in the dma instance for this channel.
	 */
	dma->callback = callback;

	/*
	 *  Save the user data in the the dma instance for this channel.
	 */
	dma->user_data = user_data;

	/*
	 *  Put the dma instance into the pause state by setting the
	 *  pause bit to true.
	 */
	dma->pause = TRUE;

	local_irq_restore(flags);

	/*
	 *  Success.
	 */
	return(0);
}

/*****************************************************************************
 *
 *  int dma_start(int handle, unsigned int channels, unsigned int * handles)
 *
 *  Description: Initiate a transfer on up to 3 channels.
 *
 *  handle:	 handle for the channel to initiate transfer on.
 *  channels:   number of channels to initiate transfers on.
 *  handles:	pointer to an array of handles, one for each channel which
 *			   is to be started.
 *
 ****************************************************************************/
static int
dma_start_m2m(int channel, ep93xx_dma_t * dma)
{
	unsigned int flags;
	unsigned int M2M_reg_base = dma->reg_base;
	unsigned int uiCONTROL;

	/*
	 *  Mask interrupts while we get this started.
	 */
	local_irq_save(flags);

	/*
	 *  Make sure the channel has at least one buffer in the queue.
	 */
	if (dma->new_buffers < 1) {
		/*
		 *  Unmask irqs
		 */
		local_irq_restore(flags);

		DPRINTK("DMA Start: Channel starved.\n");

		/*
		 *  This channel does not have enough buffers queued up,
		 *  so enter the pause by starvation state.
		 */
		dma->xfer_enable = TRUE;
		dma->pause = TRUE;

		/*
		 *  Success.
		 */
		return(0);
	}

	/*
	 *  Clear any pending interrupts.
	 */
	outl(0x0, M2M_reg_base+M2M_OFFSET_INTERRUPT);

	/*
	 *  Set up one or both buffer descriptors with values from the next one or
	 *  two buffers in the queue.  By default disable the next frame buffer
	 *  interrupt on the channel.
	 */
	uiCONTROL = inl(M2M_reg_base+M2M_OFFSET_CONTROL);
	uiCONTROL &= ~CONTROL_M2M_NFBINTEN;
	outl( uiCONTROL, M2M_reg_base+M2M_OFFSET_CONTROL );

	/*
	 * enable the done interrupt.
	 */
	uiCONTROL = inl(M2M_reg_base+M2M_OFFSET_CONTROL);
	uiCONTROL |= CONTROL_M2M_DONEINTEN;
	outl( uiCONTROL, M2M_reg_base+M2M_OFFSET_CONTROL );

	/*
	 *  Update the dma channel instance transfer state.
	 */
	dma->xfer_enable = TRUE;
	dma->pause = FALSE;

	/*
	 *  Program up the first buffer descriptor with a source and destination
	 *  and a byte count.
	 */
	outl( dma->buffer_queue[dma->current_buffer].source,
	      M2M_reg_base+M2M_OFFSET_SAR_BASE0 );

	outl( dma->buffer_queue[dma->current_buffer].dest,
	      M2M_reg_base+M2M_OFFSET_DAR_BASE0 );

	outl( dma->buffer_queue[dma->current_buffer].size,
	      M2M_reg_base+M2M_OFFSET_BCR0 );

	/*
	 *  Decrement the new buffers counter.
	 */
	dma->new_buffers--;

	/*
	 * Set up the second buffer descriptor with a second buffer if we have
	 * a second buffer.
	 */
	if (dma->new_buffers) {
		outl( dma->buffer_queue[(dma->current_buffer + 1) %
					MAX_EP93XX_DMA_BUFFERS].source,
		      M2M_reg_base+M2M_OFFSET_SAR_BASE1 );

		outl( dma->buffer_queue[(dma->current_buffer + 1) %
					MAX_EP93XX_DMA_BUFFERS].dest,
		      M2M_reg_base+M2M_OFFSET_DAR_BASE1 );

		outl( dma->buffer_queue[(dma->current_buffer + 1) %
					MAX_EP93XX_DMA_BUFFERS].size,
		      M2M_reg_base+M2M_OFFSET_BCR1 );

		uiCONTROL = inl(M2M_reg_base+M2M_OFFSET_CONTROL);
		uiCONTROL |= CONTROL_M2M_NFBINTEN;
		outl( uiCONTROL, M2M_reg_base+M2M_OFFSET_CONTROL );

		dma->new_buffers--;
	}

	/*
	 *  Now we enable the channel.  This initiates the transfer.
	 */
	uiCONTROL = inl(M2M_reg_base+M2M_OFFSET_CONTROL);
	uiCONTROL |= CONTROL_M2M_ENABLE;
	outl( uiCONTROL, M2M_reg_base+M2M_OFFSET_CONTROL );
	inl(M2M_reg_base + M2M_OFFSET_CONTROL);

	/*
	 *  If this is a memory to memory transfer, we need to s/w trigger the
	 *  transfer by setting the start bit within the control register.
	 */
	if (dma->device == DMA_MEMORY) {
		uiCONTROL = inl(M2M_reg_base+M2M_OFFSET_CONTROL);
		uiCONTROL |= CONTROL_M2M_START;
		outl( uiCONTROL, M2M_reg_base+M2M_OFFSET_CONTROL );
	}

	DPRINTK("DMA - It's been started!!");
	DPRINTK("CONTROL - 0x%x \n",	inl(M2M_reg_base+M2M_OFFSET_CONTROL) );
	DPRINTK("STATUS - 0x%x \n",	 inl(M2M_reg_base+M2M_OFFSET_STATUS) );
	DPRINTK("BCR0 - 0x%x \n",	   dma->buffer_queue[dma->current_buffer].size);
	DPRINTK("SAR_BASE0 - 0x%x \n",  inl(M2M_reg_base+M2M_OFFSET_SAR_BASE0) );
	DPRINTK("SAR_CUR0 - 0x%x \n",   inl(M2M_reg_base+M2M_OFFSET_SAR_CURRENT0) );
	DPRINTK("DAR_BASE0 - 0x%x \n",  inl(M2M_reg_base+M2M_OFFSET_DAR_BASE0) );
	DPRINTK("DAR_CUR0 - 0x%x \n",   inl(M2M_reg_base+M2M_OFFSET_DAR_CURRENT0) );

	/*
	 *  Unmask irqs
	 */
	local_irq_restore(flags);

	/*
	 *  Success.
	 */
	return(0);
}

/*****************************************************************************
 *
 *  DMA interface functions
 *
 ****************************************************************************/

/*****************************************************************************
 *
 *  int dma_init(int handle, unsigned int flags_m2p, unsigned int flags_m2m,
 *			   dma_callback callback, unsigned int user_data)
 *
 *  Description: Configure the DMA channel and install a callback function.
 *
 *  handle:	 Handle unique the each instance of the dma interface, used
 *			  to verify this call.
 *  flags_m2p   Flags used to configure an M2P/P2M dma channel and determine
 *			  if a callback function and user_data information are included
 *			  in this call. This field should be NULL if handle represents
 *			  an M2M channel.
 *  flags_m2m   Flags used to configure an M2M dma channel and determine
 *			  if a callback function and user_data information are included
 *			  in this call. This field should be NULL if handle represents
 *			  an M2P/P2M channel.
 *  callback	function pointer which is called near the end of the
 *			  dma channel's irq handler.
 *  user_data   defined by the calling driver.
 *
 ****************************************************************************/
int
ep93xx_dma_config(int handle, unsigned int flags_m2p, unsigned int flags_m2m,
		  dma_callback callback, unsigned int user_data)
{
	int  channel;
	ep93xx_dma_t * dma;
	int flags;
	unsigned int M2P_reg_base, uiCONTROL;

	/*
	 *  Get the DMA hw channel # from the handle.
	 */
	channel = dma_get_channel_from_handle(handle);

	/*
	 *  See if this is a valid handle.
	 */
	if (channel < 0) {
		printk(KERN_ERR
			   "DMA Config: Invalid dma handle.\n");
		return(-EINVAL);
	}

	DPRINTK("DMA Config \n");

	dma = &dma_chan[channel];

	local_irq_save(flags);

	/*
	 *  Check if the channel is currently transferring.
	 */
	if (dma->xfer_enable) {
		local_irq_restore(flags);
		return(-EINVAL);
	}

	/*
	 *  Check if this is an m2m function.
	 */
	if (channel >= 10) {
		local_irq_restore(flags);

		/*
		 *  Call another function to handle m2m config.
		 */
		return(dma_config_m2m(dma, flags_m2m, callback, user_data));
	}

	/*
	 *  Setup a pointer into the dma channel's register set.
	 */
	M2P_reg_base = dma->reg_base;

	/*
	 *  By default we enable the stall interrupt.
	 */
	uiCONTROL = inl(M2P_reg_base+M2P_OFFSET_CONTROL);
	uiCONTROL |= CONTROL_M2P_STALLINTEN;
	outl( uiCONTROL, M2P_reg_base+M2P_OFFSET_CONTROL );

	/*
	 *  Configure the channel for an error from the peripheral.
	 */
	uiCONTROL = inl(M2P_reg_base+M2P_OFFSET_CONTROL);
	if ( flags_m2p && CHANNEL_ERROR_INT_ENABLE )
		uiCONTROL |= CONTROL_M2P_CHERRORINTEN;
	else
		uiCONTROL &= ~CONTROL_M2P_CHERRORINTEN;
	outl( uiCONTROL, M2P_reg_base+M2P_OFFSET_CONTROL );

	uiCONTROL = inl(M2P_reg_base+M2P_OFFSET_CONTROL);
	if ( flags_m2p && CHANNEL_ABORT )
		uiCONTROL |= CONTROL_M2P_ABRT;
	else
		uiCONTROL &= ~CONTROL_M2P_ABRT;
	outl( uiCONTROL, M2P_reg_base+M2P_OFFSET_CONTROL );

	uiCONTROL = inl(M2P_reg_base+M2P_OFFSET_CONTROL);
	if ( flags_m2p && IGNORE_CHANNEL_ERROR )
		uiCONTROL |= CONTROL_M2P_ICE;
	else
		uiCONTROL &= ~CONTROL_M2P_ICE;
	outl( uiCONTROL, M2P_reg_base+M2P_OFFSET_CONTROL );

	/*
	 *  Save the callback function in the dma instance for this channel.
	 */
	dma->callback = callback;

	/*
	 *  Save the user data in the the dma instance for this channel.
	 */
	dma->user_data = user_data;

	/*
	 *  Put the dma instance into the pause state by setting the
	 *  pause bit to true.
	 */
	dma->pause = TRUE;

	local_irq_restore(flags);

	/*
	 *  Success.
	 */
	return(0);
}

/*****************************************************************************
 *
 *  int dma_start(int handle, unsigned int channels, unsigned int * handles)
 *
 *  Description: Initiate a transfer on up to 3 channels.
 *
 *  handle:	 handle for the channel to initiate transfer on.
 *  channels:   number of channels to initiate transfers on.
 *  handles:	pointer to an array of handles, one for each channel which
 *			   is to be started.
 *
 ****************************************************************************/
int
ep93xx_dma_start(int handle, unsigned int channels, unsigned int * handles)
{
	ep93xx_dma_t * dma_pointers[3];
	unsigned int M2P_reg_bases[3];
	unsigned int loop, flags, uiCONTROL;
	int  channel;

	/*
	 *  Get the DMA hw channel # from the handle.
	 */
	channel = dma_get_channel_from_handle(handle);

	/*
	 *  See if this is a valid handle.
	 */
	if (channel < 0) {
		printk(KERN_ERR "DMA Start: Invalid dma handle.\n");
		return(-EINVAL);
	}

	if (channels < 1) {
		printk(KERN_ERR "DMA Start: Invalid parameter.\n");
		return(-EINVAL);
	}

	DPRINTK("DMA Start \n");

	/*
	 *  Mask off registers.
	 */
	local_irq_save(flags);

	/*
	 *  Check if this is a start multiple.
	 */
	if (channels > 1) {
		DPRINTK("DMA ERROR: Start, multiple start not supported yet \n");
		return(-1);
	} else {
		/*
		 *  Check if this channel is already transferring.
		 */
		if (dma_chan[channel].xfer_enable && !dma_chan[channel].pause) {
			printk(KERN_ERR
				   "DMA Start: Invalid command for channel %d.\n", channel);

			/*
			 *  Unmask irqs
			 */
			local_irq_restore(flags);

			/*
			 *  This channel is already transferring, so return an error.
			 */
			return(-EINVAL);
		}

		/*
		 *  If this is an M2M channel, call a different function.
		 */
		if (channel >= 10) {
			/*
			 *  Unmask irqs
			 */
			local_irq_restore(flags);

			/*
			 *  Call the m2m start function.  Only start one channel.
			 */
			return(dma_start_m2m(channel, &dma_chan[channel]));
		}

		/*
		 *  Make sure the channel has at least one buffer in the queue.
		 */
		if (dma_chan[channel].new_buffers < 1) {
			DPRINTK("DMA Start: Channel starved.\n");

			/*
			 *  This channel does not have enough buffers queued up,
			 *  so enter the pause by starvation state.
			 */
			dma_chan[channel].xfer_enable = TRUE;
			dma_chan[channel].pause = TRUE;

			/*
			 *  Unmask irqs
			 */
			local_irq_restore(flags);

			/*
			 *  Success.
			 */
			return(0);
		}

		/*
		 *  Set up a dma instance pointer for this dma channel.
		 */
		dma_pointers[0] = &dma_chan[channel];

		/*
		 * Set up a pointer to the register set for this channel.
		 */
		M2P_reg_bases[0] = dma_pointers[0]->reg_base;
	}

	/*
	 *  Setup both MAXCNT registers with values from the next two buffers
	 *  in the queue, and enable the next frame buffer interrupt on the channel.
	 */
	for (loop = 0; loop < channels; loop++) {
		/*
		 *  By default we enable the next frame buffer interrupt.
		 */
		uiCONTROL = inl(M2P_reg_bases[loop]+M2P_OFFSET_CONTROL);
		uiCONTROL |= CONTROL_M2P_NFBINTEN;
		outl( uiCONTROL, M2P_reg_bases[loop]+M2P_OFFSET_CONTROL );

		/*
		 *  Check if we need to restore a paused transfer.
		 */
		if (dma_pointers[loop]->pause_buf.buf_id != -1)
			outl( dma_pointers[loop]->pause_buf.size,
			      M2P_reg_bases[loop]+M2P_OFFSET_MAXCNT0 );
		else
			outl( dma_pointers[loop]->buffer_queue[dma_pointers[loop]->current_buffer].size,
			      M2P_reg_bases[loop]+M2P_OFFSET_MAXCNT0 );
	}

	for (loop = 0; loop < channels; loop++) {
		/*
		 *  Enable the specified dma channels.
		 */
		uiCONTROL = inl(M2P_reg_bases[loop]+M2P_OFFSET_CONTROL);
		uiCONTROL |= CONTROL_M2P_ENABLE;
		outl( uiCONTROL, M2P_reg_bases[loop]+M2P_OFFSET_CONTROL );

		/*
		 *  Update the dma channel instance transfer state.
		 */
		dma_pointers[loop]->xfer_enable = TRUE;
		dma_pointers[loop]->pause = FALSE;

		if (dma_pointers[loop]->pause_buf.buf_id == -1)
			dma_pointers[loop]->new_buffers--;
	}

	/*
	 *  Program up the BASE0 registers for all specified channels, this
	 *  will initiate transfers on all specified channels.
	 */
	for (loop = 0; loop < channels; loop++)
		/*
		 *  Check if we need to restore a paused transfer.
		 */
		if (dma_pointers[loop]->pause_buf.buf_id != -1) {
			outl( dma_pointers[loop]->pause_buf.source,
			      M2P_reg_bases[loop]+M2P_OFFSET_BASE0 );

			/*
			 *  Set the pause buffer to NULL
			 */
			dma_pointers[loop]->pause_buf.buf_id = -1;
			dma_pointers[loop]->pause_buf.size = 0;
		} else
			outl( dma_pointers[loop]->buffer_queue[
				  dma_pointers[loop]->current_buffer].source,
			      M2P_reg_bases[loop]+M2P_OFFSET_BASE0 );

	/*
	 *  Before restoring irqs setup the second MAXCNT/BASE
	 *  register with a second buffer.
	 */
	for (loop = 0; loop < channels; loop++)
		if (dma_pointers[loop]->new_buffers) {
			outl( dma_pointers[loop]->buffer_queue[
				  (dma_pointers[loop]->current_buffer + 1) %
				  MAX_EP93XX_DMA_BUFFERS].size,
			      M2P_reg_bases[loop]+M2P_OFFSET_MAXCNT1 );

			outl( dma_pointers[loop]->buffer_queue[
				  (dma_pointers[loop]->current_buffer + 1) %
				  MAX_EP93XX_DMA_BUFFERS].source,
			      M2P_reg_bases[loop]+M2P_OFFSET_BASE1 );
		}

	/*
	  DPRINTK("DMA - It's been started!!");
	  DPRINTK("STATUS - 0x%x \n",	 inl(M2P_reg_base+M2P_OFFSET_STATUS) );
	  DPRINTK("CONTROL - 0x%x \n",	inl(M2P_reg_base+M2P_OFFSET_CONTROL) );
	  DPRINTK("REMAIN - 0x%x \n",	 inl(M2P_reg_base+M2P_OFFSET_REMAIN) );
	  DPRINTK("PPALLOC - 0x%x \n",	inl(M2P_reg_base+M2P_OFFSET_PPALLOC) );
	  DPRINTK("BASE0 - 0x%x \n",	  inl(M2P_reg_base+M2P_OFFSET_BASE0) );
	  DPRINTK("MAXCNT0 - 0x%x \n",	inl(M2P_reg_base+M2P_OFFSET_MAXCNT0) );
	  DPRINTK("CURRENT0 - 0x%x \n",   inl(M2P_reg_base+M2P_OFFSET_CURRENT0) );
	  DPRINTK("BASE1 - 0x%x \n",	  inl(M2P_reg_base+M2P_OFFSET_BASE1) );
	  DPRINTK("MAXCNT1 - 0x%x \n",	inl(M2P_reg_base+M2P_OFFSET_MAXCNT1) );
	  DPRINTK("CURRENT1 - 0x%x \n",   inl(M2P_reg_base+M2P_OFFSET_CURRENT1) );

	  DPRINTK("Pause - %d \n", dma_pointers[0]->pause);
	  DPRINTK("xfer_enable - %d \n", dma_pointers[0]->xfer_enable);
	  DPRINTK("total bytes - 0x%x \n", dma_pointers[0]->total_bytes);
	  DPRINTK("total buffer - %d \n", dma_pointers[0]->total_buffers);
	  DPRINTK("new buffers - %d \n", dma_pointers[0]->new_buffers);
	  DPRINTK("current buffer - %d \n", dma_pointers[0]->current_buffer);
	  DPRINTK("last buffer - %d \n", dma_pointers[0]->last_buffer);
	  DPRINTK("used buffers - %d \n", dma_pointers[0]->used_buffers);
	*/
	/*
	 *  Unmask irqs
	 */
	local_irq_restore(flags);

	/*
	 *  Success.
	 */
	return(0);
}

/*****************************************************************************
 *
 *  int ep93xx_dma_add_buffer(int handle, unsigned int * address,
 *						 unsigned int size, unsigned int last)
 *
 *  Description: Add a buffer entry to the DMA buffer queue.
 *
 *  handle:	 handle for the channel to add this buffer to.
 *  address:	Pointer to an integer which is the start address of the
 *			  buffer which is to be added to the queue.
 *  size:	   size of the buffer in bytes.
 *  last:	   1 if this is the last buffer in this stream, 0 otherwise.
 *
 ****************************************************************************/
int
ep93xx_dma_add_buffer(int handle, unsigned int source, unsigned int dest,
		      unsigned int size, unsigned int last,
		      unsigned int buf_id)
{
	unsigned int flags;
	ep93xx_dma_t * dma;
	int  channel;

	/*
	 *  Get the DMA hw channel # from the handle.
	 */
	channel = dma_get_channel_from_handle(handle);

	/*
	 *  See if this is a valid handle.
	 */
	if (channel < 0) {
		printk(KERN_ERR
			   "DMA Add Buffer: Invalid dma handle.\n");
		return(-EINVAL);
	}

	/*
	 *  Get a pointer to the dma instance.
	 */
	dma = &dma_chan[channel];

	/*
	 *  Mask interrupts and hold on to the original state.
	 */
	local_irq_save(flags);

	/*
	 *  If the buffer queue is full, last_buffer is the same as current_buffer and
	 *  we're not tranfering, or last_buffer is pointing to a used buffer, then exit.
	 *  TODO: do I need to do any more checks?
	 */
	if (dma->total_buffers >= MAX_EP93XX_DMA_BUFFERS) {
		/*
		 *  Restore the state of the irqs
		 */
		local_irq_restore(flags);

		/*
		 *  Fail.
		 */
		return(-1);
	}

	/*
	 *  Add this buffer to the queue
	 */
	dma->buffer_queue[dma->last_buffer].source = source;
	dma->buffer_queue[dma->last_buffer].dest = dest;
	dma->buffer_queue[dma->last_buffer].size = size;
	dma->buffer_queue[dma->last_buffer].last = last;
	dma->buffer_queue[dma->last_buffer].buf_id = buf_id;

	/*
	 *  Reset the used field of the buffer structure.
	 */
	dma->buffer_queue[dma->last_buffer].used = FALSE;

	/*
	 *  Increment the End Item Pointer.
	 */
	dma->last_buffer = (dma->last_buffer + 1) % MAX_EP93XX_DMA_BUFFERS;

	/*
	 *  Increment the new buffers counter and the total buffers counter
	 */
	dma->new_buffers++;
	dma->total_buffers++;

	/*
	 *  restore the interrupt state.
	 */
	local_irq_restore(flags);

	/*
	 *  Check if the channel was starved into a stopped state.
	 */
	if (dma->pause && dma->xfer_enable) {
		if (dma->new_buffers >= 1) {
			DPRINTK("DMA - calling start from add after starve. \n");

			/*
			 *  The channel was starved into a stopped state, and we've got
			 *  2 new buffers, so start tranferring again.
			 */
			ep93xx_dma_start(handle, 1, 0);
		}
	}

	/*
	 *  Success.
	 */
	return(0);
}

/*****************************************************************************
 *
 *  int ep93xx_dma_remove_buffer(int handle, unsigned int * address,
 *								unsigned int * size)
 *
 *  Description: Remove a buffer entry from the DMA buffer queue. If
 *			   buffer was removed successfully, return 0, otherwise
 *			   return -1.
 *
 *  handle:	 handle for the channel to remove a buffer from.
 *  address:	Pointer to an integer which is filled in with the start
 *			  address of the removed buffer.
 *  size:	   Pointer to an integer which is filled in with the size in
 *			  bytes of the removed buffer.
 *
 ****************************************************************************/
int
ep93xx_dma_remove_buffer(int handle, unsigned int * buf_id)
{
	unsigned int test;
	unsigned int loop;
	int return_val = -1;
	unsigned int flags;
	ep93xx_dma_t *dma;
	int  channel;

	/*
	 *  Get the DMA hw channel # from the handle.
	 */
	channel = dma_get_channel_from_handle(handle);

	/*
	 *  See if this is a valid handle.
	 */
	if (channel < 0) {
		printk(KERN_ERR
			   "DMA Remove Buffer: Invalid dma handle.\n");
		return(-EINVAL);
	}

	dma = &dma_chan[channel];

	/*
	 *  Mask interrupts and hold on to the original state.
	 */
	local_irq_save(flags);

	/*
	 *  Make sure there are used buffers to be returned.
	 */
	if (dma->used_buffers) {
		test = dma->last_buffer;

		for (loop = 0; loop < MAX_EP93XX_DMA_BUFFERS; loop++) {
			if (dma->buffer_queue[test].used && (dma->buffer_queue[test].buf_id != -1)) {
				/*DPRINTK("buffer %d used \n", test); */

				/*
				 *  This is a used buffer, fill in the buf_id pointer
				 *  with the buf_id for this buffer.
				 */
				*buf_id = dma->buffer_queue[test].buf_id;

				/*
				 *  Reset this buffer structure
				 */
				dma->buffer_queue[test].buf_id = -1;

				/*
				 *  Decrement the used buffer counter, and the total buffer counter.
				 */
				dma->used_buffers--;
				dma->total_buffers--;

				/*
				 *  Successful removal of a buffer, so set the return
				 *  value to 0, then exit this loop.
				 */
				return_val = 0;
				break;
			}

			/*
			 *  This buffer isn't used, let's see if the next one is.
			 */
			test = (test + 1) % MAX_EP93XX_DMA_BUFFERS;
		}
	}

	/*
	 *  Restore interrupts.
	 */
	local_irq_restore(flags);

	/*
	 *  Success.
	 */
	return(return_val);
}

/*****************************************************************************
 *
 *  int ep93xx_dma_pause(int handle, unsigned int channels,
 *					   unsigned int * handles)
 *
 *  Description: Disable any ongoing transfer for the given channel, retaining
 *			   the state of the current buffer transaction so that upon
 *			   resume, the dma will continue where it left off.
 *
 *  handle:	 Handle for the channel to be paused.  If this is a pause for
 *			  for multiple channels, handle is a valid handle for one of
 *			  the channels to be paused.
 *  channels:   number of channel to pause transfers on.
 *  handles:	Pointer to an array of handles, one for each channel which
 *			  to be paused.  If this pause is intended only for one
 *			  channel, this field should be set to NULL.
 *
 ****************************************************************************/
int
ep93xx_dma_pause(int handle, unsigned int channels, unsigned int * handles)
{
	unsigned int flags;
	ep93xx_dma_t * dma;
	int channel;

	DPRINTK("ep93xx_dma_pause \n");

	/*
	 *  Mask interrupts and hold on to the original state.
	 */
	local_irq_save(flags);

	/*
	 *  Get the DMA hw channel # from the handle.
	 */
	channel = dma_get_channel_from_handle(handle);

	/*
	 *  See if this is a valid handle.
	 */
	if (channel < 0) {
		/*
		 *  restore interrupts.
		 */
		local_irq_restore(flags);

		printk(KERN_ERR
			   "DMA Pause: Invalid dma handle.\n");

		/*
		 *  Fail.
		 */
		return(-EINVAL);
	}

	DPRINTK("DMA %d: pause \n", channel);

	/*
	 *  Set up a pointer to the dma instance data.
	 */
	dma = &dma_chan[channel];

	/*
	 *  Check if we're already paused.
	 */
	if (dma->pause) {
		/*
		 *  We're paused, but are we stopped?
		 */
		if (dma->xfer_enable)
			/*
			 *  Put the channel in the stopped state.
			 */
			dma->xfer_enable = FALSE;

		DPRINTK("DMA Pause - already paused.");
	} else {
		/*
		 *  Put the channel into the stopped state.
		 */
		dma->xfer_enable = FALSE;
		dma->pause = TRUE;
	}

	/*
	 *  restore interrupts.
	 */
	local_irq_restore(flags);

	/*
	 *  Already paused, so exit.
	 */
	return(0);
}

/*****************************************************************************
 *
 *  void ep93xx_dma_flush(int handle)
 *
 *  Description: Flushes all queued buffers and transfers in progress
 *			   for the given channel.  Return the buffer entries
 *			   to the calling function.
 *
 *  handle:	 handle for the channel for which the flush is intended.
 *
 ****************************************************************************/
int
ep93xx_dma_flush(int handle)
{
	unsigned int loop, flags;
	ep93xx_dma_t * dma;
	int  channel;
	unsigned int M2P_reg_base;

	/*
	 *  Get the DMA hw channel # from the handle.
	 */
	channel = dma_get_channel_from_handle(handle);

	/*
	 *  See if this is a valid handle.
	 */
	if (channel < 0) {
		printk(KERN_ERR "DMA Flush: Invalid dma handle.\n");
		return(-EINVAL);
	}

	DPRINTK("DMA %d: flush \n", channel);

	/*
	 *  Set up a pointer to the dma instance data for this channel
	 */
	dma = &dma_chan[channel];

	/*
	 *  Mask interrupts and hold on to the original state.
	 */
	local_irq_save(flags);

	for (loop = 0; loop < MAX_EP93XX_DMA_BUFFERS; loop++)
		dma->buffer_queue[loop].buf_id = -1;

	/*
	 *  Set the Current and Last item to zero.
	 */
	dma->current_buffer = 0;
	dma->last_buffer = 0;

	/*
	 *  Reset the Buffer counters
	 */
	dma->used_buffers = 0;
	dma->new_buffers = 0;
	dma->total_buffers = 0;

	/*
	 *  reset the Total bytes counter.
	 */
	dma->total_bytes = 0;

	M2P_reg_base = dma_chan[channel].reg_base;

	/*
	 *  restore interrupts.
	 */
	local_irq_restore(flags);

	/*
	 *  Success.
	 */
	return(0);
}

/*****************************************************************************
 *
 *  int ep93xx_dma_queue_full(int handle)
 *
 *  Description: Query to determine if the DMA queue of buffers for
 *			  a given channel is full.
 *			  0 = queue is full
 *			  1 = queue is not full
 *
 *  handle:	 handle for the channel to query.
 *
 ****************************************************************************/
int
ep93xx_dma_queue_full(int handle)
{
	int list_full = 0;
	unsigned int flags;
	int  channel;

	/*
	 *  Get the DMA hw channel # from the handle.
	 */
	channel = dma_get_channel_from_handle(handle);

	/*
	 *  See if this is a valid handle.
	 */
	if (channel < 0) {
		printk(KERN_ERR "DMA Queue Full: Invalid dma handle.\n");
		return(-EINVAL);
	}

	DPRINTK("DMA %d: queue full \n", channel);

	/*
	 *  Mask interrupts and hold on to the original state.
	 */
	local_irq_save(flags);

	/*
	 *  If the last item is equal to the used item then
	 *  the queue is full.
	 */
	if (dma_chan[channel].total_buffers < MAX_EP93XX_DMA_BUFFERS)
		list_full =  FALSE;
	else
		list_full = TRUE;

	/*
	 *  restore interrupts.
	 */
	local_irq_restore(flags);

	return(list_full);
}

/*****************************************************************************
 *
 *  int ep93xx_dma_get_position()
 *
 *  Description:  Takes two integer pointers and fills them with the start
 *                and current address of the buffer currently transferring
 *                on the specified DMA channel.
 *
 *  handle         handle for the channel to query.
 *  *buf_id        buffer id for the current buffer transferring on the
 *                 dma channel.
 *  *total         total bytes transferred on the channel.  Only counts  
 *                 whole buffers transferred.
 *  *current_frac  number of bytes transferred so far in the current buffer.
 ****************************************************************************/
int
ep93xx_dma_get_position(int handle, unsigned int * buf_id,
                        unsigned int * total, unsigned int * current_frac )
{
	int  channel;
	ep93xx_dma_t * dma;
	unsigned int buf_id1, total1, current_frac1, buf_id2, total2;
	unsigned int Status, NextBuffer, StateIsBufNext, M2P_reg_base=0;
	unsigned int pause1, pause2;

	/*
	 *  Get the DMA hw channel # from the handle.  See if this is a 
	 *  valid handle.
	 */
	channel = dma_get_channel_from_handle(handle);
	if (channel < 0) {
		printk(KERN_ERR "DMA Get Position: Invalid dma handle.\n");
		return(-EINVAL);
	}

	dma = &dma_chan[channel];

	/*
	 * If DMA moves to a new buffer in the middle of us grabbing the 
	 * buffer info, then do it over again.
	 */
	do{
		buf_id1 = dma->buffer_queue[dma->current_buffer].buf_id;
		total1  = dma->total_bytes;
		pause1  = dma->pause;

		if (channel < 10) {
			// M2P
			M2P_reg_base = dma->reg_base;

			Status = inl(M2P_reg_base+M2P_OFFSET_STATUS);

			NextBuffer = ((Status & STATUS_M2P_NEXTBUFFER) != 0);

			StateIsBufNext = ((Status & STATUS_M2P_CURRENT_MASK) == 
			                  STATUS_M2P_DMA_BUF_NEXT);
			
			if( NextBuffer ^ StateIsBufNext )
				current_frac1 = inl(M2P_reg_base+M2P_OFFSET_CURRENT1) -
				                inl(M2P_reg_base+M2P_OFFSET_BASE1);	
			else
				current_frac1 = inl(M2P_reg_base+M2P_OFFSET_CURRENT0) -
				                inl(M2P_reg_base+M2P_OFFSET_BASE0);	
			
		} else { 
			// M2M - TODO implement this for M2M
			current_frac1 = 0;
		}
		
		buf_id2 = dma->buffer_queue[dma->current_buffer].buf_id;
		total2 = dma->total_bytes;
		pause2  = dma->pause;

	} while ( (buf_id1 != buf_id2) || (total1 != total2) || (pause1 != pause2) );

	if (pause1)
		current_frac1 = 0;

	if (buf_id)
		*buf_id = buf_id1;

	if (total)
		*total  = total1;

	if (current_frac)
		*current_frac = current_frac1;
	
//	DPRINTK("DMA buf_id %d, total %d, frac %d\n", buf_id1, total1, current_frac1);

	/*
	 *  Success.
	 */
	return(0);
}

/*****************************************************************************
 *
 *  int ep93xx_dma_get_total(int handle)
 *
 *  Description:	Returns the total number of bytes transferred on the
 *			specified channel since the channel was requested.
 *
 *  handle:	 handle for the channel to query.
 *
 ****************************************************************************/
int
ep93xx_dma_get_total(int handle)
{
	int  channel;

	/*
	 *  Get the DMA hw channel # from the handle.
	 */
	channel = dma_get_channel_from_handle(handle);

	/*
	 *  See if this is a valid handle.
	 */
	if (channel < 0) {
		printk(KERN_ERR "DMA Get Total: Invalid dma handle.\n");
		return(-EINVAL);
	}

	DPRINTK("DMA %d: total: %d \n", channel, dma_chan[channel].total_bytes);

	/*
	 *  Return the total number of bytes transferred on this channel since
	 *  it was requested.
	 */
	return(dma_chan[channel].total_bytes);
}

/*****************************************************************************
 *
 *  int ep93xx_dma_is_done(int handle)
 *
 *  Description:	Determines if the specified channel is done
 *			transferring the requested data.
 *
 *  handle:	 handle for the channel to query.
 *
 ****************************************************************************/
int
ep93xx_dma_is_done(int handle)
{
	ep93xx_dma_t *dma;
	int channel;

	/*
	 *  Get the DMA hw channel # from the handle.
	 */
	channel = dma_get_channel_from_handle(handle);

	/*
	 *  See if this is a valid handle.
	 */
	if (channel < 0) {
		printk(KERN_ERR "ep93xx_dma_is_done: Invalid dma handle.\n");
		return(-EINVAL);
	}

        /*
         * Get a pointer to the DMA channel state structure.
         */
        dma = &dma_chan[channel];

        /*
         * See if there are any buffers remaining to be provided to the HW.
         */
        if (dma->new_buffers)
            return 0;

        /*
         * See if this is a M2P or M2M channel.
         */
        if (channel < 10) {
            /*
             * If the bytes remaining register of the HW is not zero, then
             * there is more work to be done.
             */
            if (inl(dma->reg_base + M2P_OFFSET_REMAIN) != 0)
                return 0;
        } else {
            /*
             * If either byte count register in the HW is not zero, then there
             * is more work to be done.
             */
            if ((inl(dma->reg_base + M2M_OFFSET_BCR0) != 0) ||
                (inl(dma->reg_base + M2M_OFFSET_BCR1) != 0))
                return 0;
        }

        /*
         * The DMA is complete.
         */
        return 1;
}

/*****************************************************************************
 * ep93xx_dma_request
 *
 * Description: This function will allocate a DMA channel for a particular
 * hardware peripheral.  Before initiating a transfer on the allocated
 * channel, the channel must be set up and buffers have to queued up.
 *
 *  handle:	 pointer to an integer which is filled in with a unique
 *			  handle for this instance of the dma interface.
 *  device_id   string with the device name, primarily used by /proc.
 *  device	  hardware device ID for which the requested dma channel will
 *			  transfer data.
 *
 ****************************************************************************/
int
ep93xx_dma_request(int * handle, const char *device_id,
				   ep93xx_dma_dev_t device)
{
	ep93xx_dma_t *dma = NULL;
	int channel;
	unsigned int error = 0;
	unsigned int loop;
	unsigned int M2P_reg_base;

	/*
	 *  Check if the device requesting a DMA channel is a valid device.
	 */
	if ((device >= UNDEF) || (device < 0))
		return(-ENODEV);

	/*
	 *  We've got a valid hardware device requesting a DMA channel.
	 *  Now check if the device should open an M2P or M2M channel
	 */
	if (device < 20)
		channel = dma_open_m2p(device);
	else
		channel = dma_open_m2m(device);

	/*
	 *  Check if we successfully opened a DMA channel
	 */
	if (channel < 0) {
		printk(KERN_ERR "%s: Could not open dma channel for this device.\n",
			   device_id);
		return(-EBUSY);
	}

	dma = &dma_chan[channel];

	/*
	 *  Request the appropriate IRQ for the specified channel
	 */
	if (channel < 10)
		error = request_irq(dma->irq, dma_m2p_irq_handler,
				    SA_INTERRUPT, device_id, (void *) dma);
	else
		error = request_irq(dma->irq, &dma_m2m_irq_handler,
				    SA_INTERRUPT, device_id, (void *) dma);

	/*
	 *  Check for any errors during the irq request
	 */
	if (error) {
		printk(KERN_ERR "%s: unable to request IRQ %d for DMA channel\n",
			   device_id, dma->irq);
		return(error);
	}

	/*
	 *  Generate a valid handle and exit.
	 *
	 *  Increment the last valid handle.
	 *  Check for wraparound (unlikely, but we like to be complete).
	 */
	dma->last_valid_handle++;

	if ( (dma->last_valid_handle & DMA_HANDLE_SPECIFIER_MASK) !=
	     (channel << 28) )
		dma->last_valid_handle = (channel << 28) + 1;

	/*
	 *  Fill in the handle pointer with a valid handle for
	 *  this dma channel instance.
	 */
	*handle = dma->last_valid_handle;

	DPRINTK("Handle for channel %d: 0x%x\n", channel, *handle);

	/*
	 * Save the device ID and device name.
	 */
	dma->device = device;
	dma->device_id = device_id;

	/*
	 *  Init all fields within the dma instance.
	 */
	for (loop = 0; loop < MAX_EP93XX_DMA_BUFFERS; loop++)
		dma->buffer_queue[loop].buf_id = -1;

	/*
	 *  Initialize all buffer queue variables.
	 */
	dma->current_buffer = 0;
	dma->last_buffer = 0;

	dma->new_buffers = 0;
	dma->used_buffers = 0;
	dma->total_buffers = 0;

	/*
	 *  Initialize the total bytes variable
	 */
	dma->total_bytes = 0;

	/*
	 *  Initialize the transfer and pause state variables to 0.
	 */
	dma->xfer_enable = 0;

	dma->pause = 0;

	/*
	 *  Initialize the pause buffer structure.
	 */
	dma->pause_buf.buf_id = -1;

	/*
	 *  Initialize the callback function and user data fields.
	 */
	dma->callback = NULL;

	/*
	 * User data used as a parameter for the Callback function.  The user
	 * sets up the data and sends it with the callback function.
	 */
	dma->user_data = 0;

	M2P_reg_base = dma_chan[channel].reg_base;

	/*
	 *  Debugging message.
	 */
	DPRINTK("Successfully requested dma channel %d\n", channel);
	DPRINTK("STATUS - 0x%x \n",	 inl(M2P_reg_base+M2P_OFFSET_STATUS) );
	DPRINTK("CONTROL - 0x%x \n",	inl(M2P_reg_base+M2P_OFFSET_CONTROL) );
	DPRINTK("REMAIN - 0x%x \n",	 inl(M2P_reg_base+M2P_OFFSET_REMAIN) );
	DPRINTK("PPALLOC - 0x%x \n",	inl(M2P_reg_base+M2P_OFFSET_PPALLOC) );
	DPRINTK("BASE0 - 0x%x \n",	  inl(M2P_reg_base+M2P_OFFSET_BASE0) );
	DPRINTK("MAXCNT0 - 0x%x \n",	inl(M2P_reg_base+M2P_OFFSET_MAXCNT0) );
	DPRINTK("CURRENT0 - 0x%x \n",   inl(M2P_reg_base+M2P_OFFSET_CURRENT0) );
	DPRINTK("BASE1 - 0x%x \n",	  inl(M2P_reg_base+M2P_OFFSET_BASE1) );
	DPRINTK("MAXCNT1 - 0x%x \n",	inl(M2P_reg_base+M2P_OFFSET_MAXCNT1) );
	DPRINTK("CURRENT1 - 0x%x \n",   inl(M2P_reg_base+M2P_OFFSET_CURRENT1) );

	DPRINTK("Buffer	source	   size	   last	   used \n");
	for (loop = 0; loop < 5; loop ++)
		DPRINTK("%d		0x%x		 0x%x		%d		 %d \n",
			loop, dma->buffer_queue[loop].source, dma->buffer_queue[loop].size,
			dma->buffer_queue[loop].last, dma->buffer_queue[loop].used);
	DPRINTK("pause	 0x%x		 0x%x		%d		 %d \n",
		dma->pause_buf.source, dma->pause_buf.size,
		dma->pause_buf.last, dma->pause_buf.used);

	DPRINTK("Pause - %d \n", dma->pause);
	DPRINTK("xfer_enable - %d \n", dma->xfer_enable);
	DPRINTK("total bytes - 0x%x \n", dma->total_bytes);
	DPRINTK("total buffer - %d \n", dma->total_buffers);
	DPRINTK("new buffers - %d \n", dma->new_buffers);
	DPRINTK("current buffer - %d \n", dma->current_buffer);
	DPRINTK("last buffer - %d \n", dma->last_buffer);
	DPRINTK("used buffers - %d \n", dma->used_buffers);

	DPRINTK("CURRENT1 - 0x%x \n",   inl(M2P_reg_base+M2P_OFFSET_CURRENT1) );
	DPRINTK("VIC0IRQSTATUS - 0x%x, VIC0INTENABLE - 0x%x \n",
		*(unsigned int *)(VIC0IRQSTATUS),
		*(unsigned int *)(VIC0INTENABLE));

	/*
	 *  Success.
	 */
	return(0);
}

/*****************************************************************************
 *
 * ep93xx_dma_free
 *
 * Description: This function will free the dma channel for future requests.
 *
 *  handle:	 handle for the channel to be freed.
 *
 ****************************************************************************/
int
ep93xx_dma_free(int handle)
{
	ep93xx_dma_t *dma;
	unsigned int M2M_reg_base, M2P_reg_base, uiCONTROL;
	int channel;

	/*
	 *  Get the DMA hw channel # from the handle.
	 */
	channel = dma_get_channel_from_handle(handle);

	/*
	 *  See if this is a valid handle.
	 */
	if (channel < 0) {
		printk(KERN_ERR "DMA Free: Invalid dma handle.\n");
		return(-EINVAL);
	}

	/*
	 *  Get a pointer to the dma instance.
	 */
	dma = &dma_chan[channel];

	/*
	 *  Disable the dma channel
	 */
	if (channel < 10) {
		/*
		 *  M2P channel
		 */
		M2P_reg_base = dma->reg_base;

		uiCONTROL = inl(M2P_reg_base+M2P_OFFSET_CONTROL);
		uiCONTROL &= ~CONTROL_M2P_ENABLE;
		outl( uiCONTROL, M2P_reg_base+M2P_OFFSET_CONTROL );
	} else {
		/*
		 *  M2M channel
		 */
		M2M_reg_base = dma->reg_base;

		uiCONTROL = inl(M2M_reg_base+M2M_OFFSET_CONTROL);
		uiCONTROL &= ~CONTROL_M2M_ENABLE;
		outl( uiCONTROL, M2M_reg_base+M2M_OFFSET_CONTROL );
	}

	/*
	 *  Free the interrupt servicing this dma channel
	 */
	free_irq(dma->irq, (void *) dma);

	/*
	 *  Decrement the reference count for this instance of the dma interface
	 */
	dma->ref_count--;

	/*
	 *  Set the transfer and pause state variables to 0
	 *  (unititialized state).
	 */
	dma->xfer_enable = 0;
	dma->pause = 0;

	/*
	 *  Debugging message.
	 */
	DPRINTK("Successfully freed dma channel %d\n", channel);

	/*
	 *  Success.
	 */
	return(0);
}

/*****************************************************************************
 *
 * ep93xx_dma_init(void)
 *
 * Description: This function is called during system initialization to
 * setup the interrupt number and register set base address for each DMA
 * channel.
 *
 ****************************************************************************/
static int __init
ep93xx_dma_init(void)
{
	int channel;

	/*
	 * Init some values in each dma instance.
	 */
	for (channel = 0; channel < MAX_EP93XX_DMA_CHANNELS; channel++) {
		/*
		 *  IRQ for the specified dma channel.
		 */
		dma_chan[channel].irq = IRQ_DMAM2P0 + channel;

		/*
		 *  Initial value of the dma channel handle.
		 */
		dma_chan[channel].last_valid_handle = channel << 28;

		/*
		 *  Give the instance a pointer to the dma channel register
		 *  base.
		 */
		if (channel < 10)
			dma_chan[channel].reg_base = DMAM2PChannelBase[channel];
		else
			dma_chan[channel].reg_base = DMAM2MChannelBase[channel - 10];

		/*
		 *  Initialize the reference count for this channel.
		 */
		dma_chan[channel].ref_count = 0;
	}

	DPRINTK("DMA Interface intitialization complete\n");

	/*
	 * Success
	 */
	return 0;
}

__initcall(ep93xx_dma_init);
