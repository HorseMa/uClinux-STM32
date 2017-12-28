/*
 * linux/drivers/usbd/usb-inline.h - additional USB Device Core support
 *
 * Copyright (c) 2000, 2001, 2002 Lineo
 * Copyright (c) 2001 Hewlett Packard
 *
 * By: 
 *      Stuart Lynne <sl@lineo.com>, 
 *      Tom Rushworth <tbr@lineo.com>, 
 *      Bruce Balden <balden@lineo.com>
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <linux/interrupt.h>

/*
 * Initialize an urb_link to be a single element list.
 * If the urb_link is being used as a distinguished list head
 * the list is empty when the head is the only link in the list.
 */
static __inline__ void urb_link_init(urb_link *ul)
{
    if (ul) {
        ul->prev = ul->next = ul;
    }
}

/*
 * Detach an urb_link from a list, and set it
 * up as a single element list, so no dangling
 * pointers can be followed, and so it can be
 * joined to another list if so desired.
 *
 * Called from interrupt.
 */
static __inline__ void urb_detach_irq(struct usbd_urb *urb)
{
    if (urb) {
        urb_link *ul = &urb->link;
        ul->next->prev = ul->prev;
        ul->prev->next = ul->next;
        urb_link_init(ul);
    }
}

/*
 * Return the first urb_link in a list with a distinguished
 * head "hd", or NULL if the list is empty.  This will also
 * work as a predicate, returning NULL if empty, and non-NULL
 * otherwise.
 *
 * Called from interrupt.
 */
static __inline__ urb_link* first_urb_link_irq(urb_link *hd)
{
    urb_link *nx;
    if (NULL != hd && NULL != (nx = hd->next) && nx != hd) {
        // There is at least one element in the list
        // (besides the distinguished head).
        return(nx);
    }
    // The list is empty
    return(NULL);
}

/*
 * Return the first urb in a list with a distinguished
 * head "hd", or NULL if the list is empty.
 *
 * Called from interrupt.
 */
static __inline__ struct usbd_urb *first_urb_irq(urb_link *hd)
{
    urb_link *nx;
    if (NULL == (nx = first_urb_link_irq(hd))) {
        // The list is empty
        return(NULL);
    }
    return(p2surround(struct usbd_urb,link,nx));
}

/*
 * Detach and return the first urb in a list with a distinguished
 * head "hd", or NULL if the list is empty.
 */
static __inline__ struct usbd_urb *first_urb_detached_irq(urb_link *hd)
{
    struct usbd_urb *urb;
    if ((urb = first_urb_irq(hd))) {
	urb_detach_irq(urb);
    }
    return urb;
}

/*
 * Detach and return the first urb in a list with a distinguished
 * head "hd", or NULL if the list is empty.
 *
 * Can be called from non-interrupt context.
 *
 */
static __inline__ struct usbd_urb *first_urb_detached(urb_link *hd)
{
    struct usbd_urb *urb;
    unsigned long int_flags;

    local_irq_save(int_flags);
    urb = first_urb_detached_irq(hd);
    local_irq_restore(int_flags);
    return urb;
}


/*
 * Append an urb_link (or a whole list of
 * urb_links) to the tail of another list
 * of urb_links.
 */
static __inline__ void urb_append_irq(urb_link *hd, struct usbd_urb *urb)
{
    // XXX XXX _urb_detach(urb);
    if (hd && urb) {
        urb_link *new = &urb->link;

#ifdef C2L_SINGLETON_ONLY
        // This _assumes_ the new urb is a singleton,
        // but allows it to have an uninitialized link.
        //printk(KERN_DEBUG"urb_append: hd: %p n:%p p:%p new: %p n:%p p:%p\n", 
        //       hd, hd->next, hd->prev, new, new->next, new->prev);

        new->prev = hd->prev;
        new->next = hd;

        hd->prev->next = new;
        hd->prev = new;
#else 
        // This allows the new urb to be a list of urbs,
        // with new pointing at the first, but the link
        // must be initialized.
        // Order is important here...
        urb_link *pul = hd->prev;
        new->prev->next = hd;
        hd->prev = new->prev;
        new->prev = pul;
        pul->next = new;
#endif
    }
}



/**
 * usbd_dealloc_urb - deallocate an URB and associated buffer
 * @urb: pointer to an urb structure
 *
 * Deallocate an urb structure and associated data.
 */
static __inline__ void usbd_dealloc_urb(struct usbd_urb *urb)
{
    if (urb) {
        if (urb->buffer) {
            if (urb->function_instance && urb->function_instance->function_driver->ops->dealloc_urb_data) {
                urb->function_instance->function_driver->ops->dealloc_urb_data(urb);
            }
            else {
                kfree(urb->buffer);
            }
        }
        kfree(urb);
    }
}

/**
 * usbd_schedule_device_bh - schedule bottom half
 *
 * Schedule the device bottom half. This runs from timer tick so is equivalent
 * to interrupt but at lower priority.
 *
 */
static __inline__ void usbd_schedule_device_bh(struct usb_device_instance *device) 
{
    if (device && (device->status != USBD_CLOSING) && !device->device_bh.sync) {
        queue_task(&device->device_bh, &tq_immediate);
        mark_bh(IMMEDIATE_BH);
    }
}

/**
 * usbd_schedule_function_bh - schedule bottom half
 *
 * Schedule the function bottom half. This runs as a process.
 *
 */
static __inline__ void usbd_schedule_function_bh(struct usb_device_instance *device) 
{
    if (device && (device->status != USBD_CLOSING) && !device->function_bh.sync) {
	schedule_task(&device->function_bh);
    }
}

#if 0
/**
 * usbd_recv_urb_irq - process a received urb
 * @urb: pointer to an urb structure
 *
 * Used by a USB Bus interface driver to pass received data in a URB to the
 * appropriate USB Function driver via the endpoints receive queue.
 *
 * Called from interrupt.
 *
 */
static __inline__ void usbd_recv_urb_irq(struct usbd_urb *urb)
{
    if (urb) {
        urb->status = RECV_OK;
        urb->jiffies = jiffies;
	/* XXX XXX
        {
            unsigned long int_flags;
            local_irq_save(int_flags);
	    */
            urb_append_irq(&(urb->endpoint->rcv), urb);
	    /*
            local_irq_restore(int_flags);
        }   
	*/
        usbd_schedule_device_bh(urb->device);
    }
}
#endif

/**
 * usbd_recycle_urb - recycle a received urb
 * @urb: pointer to an urb structure
 *
 * Used by a USB Function interface driver to recycle an urb.
 *
 */
static __inline__ void usbd_recycle_urb(struct usbd_urb *urb)
{
    if (urb) {
        urb->actual_length = 0;
        urb->status = RECV_READY;
        urb->jiffies = jiffies;
        {
            unsigned long int_flags;
            local_irq_save(int_flags);
            urb_append_irq(&(urb->endpoint->rdy), urb);
            local_irq_restore(int_flags);
        }   

        if (urb->device->bus && urb->device->bus->driver->ops->device_event) {
            urb->device->bus->driver->ops->device_event
                (urb->device, DEVICE_RCV_URB_RECYCLED, 0);
        }
    }
}

/**
 * usbd_send_urb - submit a urb to send
 * @urb: pointer to an urb structure
 *
 * Used by a USB Function driver to submit data to be sent in an urb to the
 * appropriate USB Bus driver via the endpoints transmit queue.
 *
 * This is NOT called from interrupts.
 *
 */
static __inline__ int usbd_send_urb(struct usbd_urb *urb)
{
    if (urb && urb->endpoint && (urb->device->status == USBD_OK)) {
        //urb->device->urbs_queued++;
        urb->status = SEND_IN_PROGRESS;
        urb->jiffies = jiffies;
        {
            unsigned long int_flags;
            local_irq_save(int_flags);
            urb_append_irq(&(urb->endpoint->tx), urb);
            local_irq_restore(int_flags);
        }   
        return urb->device->bus->driver->ops->send_urb(urb);
    }
    printk(KERN_ERR"usbd_send_urb: !USBD_OK\n");
    return -EINVAL;
}


/* *
 * usbd_urb_sent_irq - tell function that an urb has been transmitted.
 * @urb: pointer to an urb structure
 *
 * Must be called from an interrupt or with interrupts disabled.
 *
 * Used by a USB Bus driver to pass a sent urb back to the function 
 * driver via the endpoints done queue.
 *
 * Called from interrupt.
 */
static __inline__ void usbd_urb_sent_irq(struct usbd_urb * urb, int rc)
{
    if (urb) {
        
        urb->status = rc;
        urb->jiffies = jiffies;
	/* XXX XXX
        {
            unsigned long int_flags;
            local_irq_save(int_flags);
            */
            urb_append_irq(&(urb->endpoint->done), urb);
            /*
            local_irq_restore(int_flags);
        }   
	*/
        usbd_schedule_device_bh(urb->device);
    }
}

/**
 * usbd_recv_setup - process a received urb
 * @urb: pointer to an urb structure
 *
 * Used by a USB Bus interface driver to pass received data in a URB to the
 * appropriate USB Function driver.
 */
static __inline__ int usbd_recv_setup(struct usbd_urb *urb)
{
    struct usb_function_instance *function_instance;
    int port = 0; // XXX compound devices

    if (urb && !(urb->device->status == USBD_CLOSING)) {

        //printk(KERN_DEBUG"usbd_recv_setup: %p request: %x vendor: %d\n", 
	//	urb, urb->device_request.bmRequestType, (urb->device_request.bmRequestType&USB_REQ_TYPE_MASK)!=0);

        function_instance = 
            ((urb->device_request.bmRequestType&USB_REQ_TYPE_MASK)==0) ?
            urb->device->ep0 : urb->device->function_instance_array+port;

        if (function_instance == NULL) {
            printk(KERN_ERR "usbd_recv_setup: endpoint: %d function_instance NULL\n", urb->endpoint->endpoint_address);
            return 1;
        }

        if (function_instance->function_driver->ops->recv_setup == NULL) {
            printk(KERN_ERR "usbd_recv_setup: recv_setup NULL\n");
            return 1;
        }

        return function_instance->function_driver->ops->recv_setup(urb);
    }
    printk(KERN_ERR"usbd_recv_setup: !USBD_OK\n");
    return -EINVAL;
}


/**
 * usbd_rcv_complete_irq - complete a receive
 * @endpoint:
 * @len:
 * @urb_bad:
 *
 * Called from rcv interrupt to complete.
 */
static __inline__ void usbd_rcv_complete_irq(struct usb_endpoint_instance *endpoint, int len, int urb_bad)
{

    // XXX XXX
    //return;

    if (endpoint) {
        struct usbd_urb *rcv_urb;

        //printk(KERN_ERR"usbd_rcv_complete: len: %d urb: %p\n", len, endpoint->rcv_urb);

        // if we had an urb then update actual_length, dispatch if neccessary
        if ((rcv_urb = endpoint->rcv_urb)) {

            //printk(KERN_ERR"usbd_rcv_complete: actual: %d buffer: %d\n", rcv_urb->actual_length, rcv_urb->buffer_length);

            // check the urb is ok, are we adding data less than the packetsize
            if (!urb_bad && (rcv_urb->device->status == USBD_OK) && (len <= endpoint->rcv_packetSize)) {

                // increment the received data size
                rcv_urb->actual_length += len;
                
                // if the current received data is short (less than full packetsize) which
                // indicates the end of the bulk transfer, we have received the maximum
                // transfersize, or if we do not have enough room to receive another packet 
                // then pass this data up to the function driver
                
                if (((len < endpoint->rcv_packetSize) || 
                        (rcv_urb->actual_length >= endpoint->rcv_transferSize) ||
                        (rcv_urb->actual_length >= (rcv_urb->buffer_length - endpoint->rcv_packetSize)))
                        ) 
                {
                    //printk(KERN_ERR"usbd_rcv_complete: finishing %p\n", rcv_urb);
                    endpoint->rcv_urb = NULL;

                    rcv_urb->status = RECV_OK;
                    rcv_urb->jiffies = jiffies;
                    urb_append_irq(&(rcv_urb->endpoint->rcv), rcv_urb);
                    usbd_schedule_device_bh(rcv_urb->device);

                    rcv_urb = NULL;
                }
            }
            else {

		printk(KERN_ERR"usbd_rcv_complete: RECV_ERROR actual: %d buffer: %d urb_bad: %d\n", 
			rcv_urb->actual_length, rcv_urb->buffer_length, urb_bad);

                // XXX TEST rcv_urb->actual_length = 0;
                // XXX TEST printk(KERN_ERR"too large %d %d\n", len, endpoint->rcv_packetSize);
                
                // increment the received data size
                rcv_urb->actual_length += len;
                rcv_urb->status = RECV_ERROR;
                rcv_urb->jiffies = jiffies;
                urb_append_irq(&(rcv_urb->endpoint->rcv), rcv_urb);
                usbd_schedule_device_bh(rcv_urb->device);
                rcv_urb = NULL;

            }
        }
        else {
            printk(KERN_ERR"usbd_rcv_complete: no rcv_urb!\n");
        }

        // if we don't have an urb see if we can get one
        if (!rcv_urb) {
            endpoint->rcv_urb = first_urb_detached_irq(&endpoint->rdy);
        }
    }
    else {
        printk(KERN_ERR"usbd_rcv_complete: no endpoint!\n");
    }

}


/**
 * usbd_tx_complete_irq - complete a transmit
 * @endpoint:
 *
 * Called from tx interrupt to complete.
 */
static __inline__ 
void usbd_tx_complete_irq(struct usb_endpoint_instance *endpoint, int restart)
{
    if (endpoint) {

        struct usbd_urb *tx_urb;

        // if we have a tx_urb advance or reset, finish if complete
        if ((tx_urb = endpoint->tx_urb)) {

            if (!restart) {
                endpoint->sent += endpoint->last;
            }

            //printk(KERN_ERR"usbd_tx_complete_irq: sent: %d\n", endpoint->sent);

            endpoint->last = 0;

            if ((tx_urb->device->status == USBD_OK) && 
                (endpoint->tx_urb->actual_length - endpoint->sent) <= 0) 
            {
                if (endpoint->endpoint_address) 
                {
                    usbd_urb_sent_irq(endpoint->tx_urb, SEND_FINISHED_OK);
                }

                endpoint->tx_urb = NULL;
                endpoint->last = endpoint->sent = 0;
            }
        }

        // if we do not have a tx_urb see if we can get one
        if (endpoint->endpoint_address && !endpoint->tx_urb && 
            (endpoint->tx_urb = first_urb_detached_irq(&endpoint->tx))) 
        {
            endpoint->last = endpoint->sent = 0;
        }
    }
    else {
        printk(KERN_ERR"usbd_tx_complete_irq: no endpoint!\n");
    }
}

