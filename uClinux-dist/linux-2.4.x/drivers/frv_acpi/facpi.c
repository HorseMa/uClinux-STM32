/*
 * FR-V ACPI Driver
 *
 * Copyright (c) 2004 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA  02111-1307  USA
 *
 */

#define EMU_LEN 100
#define DEBUG_PWRF 1

#include <asm/uaccess.h>
#include <linux/version.h>
#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/proc_fs.h>
#include <linux/devfs_fs_kernel.h>
#include <linux/ctype.h>
#include <linux/pm.h>

#include <asm/mb-regs.h>

#include "facpi.h"

#define ACPI_BUTTON_NOTIFY_STATUS 0x80

MODULE_AUTHOR("Red Hat GES Embedded Engineering Team");
MODULE_DESCRIPTION("Fujitsu FRV ACPI Driver");
MODULE_LICENSE("GPL");

extern asmlinkage void frv_cpu_suspend(unsigned long);

/* internal globals */
static struct proc_dir_entry *top_dir;
static struct proc_dir_entry *info_file, *event_file;

static struct proc_dir_entry *CPU_dir;
static struct proc_dir_entry *CPU_info_file, *CPU_limit_file, *CPU_performance_file;
static struct proc_dir_entry *CPU_power_file, *CPU_throttling_file;

static struct proc_dir_entry *ac_adapter_dir; 
static struct proc_dir_entry *ac_adapter_state_file; 

static struct proc_dir_entry *battery_dir, *BAT0_dir;
static struct proc_dir_entry *BAT0_state_file, *BAT0_info_file, *BAT0_alarm_file, *BAT0_dead_file;
static struct proc_dir_entry *BAT0_emulatecapacity_file;

static struct proc_dir_entry *button_dir;
static struct proc_dir_entry *button_power_dir;
static struct proc_dir_entry *button_power_PWRF_dir;
static struct proc_dir_entry *button_power_info_file, *button_power_state_file;

static struct proc_dir_entry *bios_dir;
static struct proc_dir_entry *bios_suspend_timeout_file;
static struct proc_dir_entry *bios_standby_timeout_file;
static struct proc_dir_entry *bios_restart_timeout_file;
static struct proc_dir_entry *bios_powerdown_mode_file;
static struct proc_dir_entry *bios_powersave_mode_file;

static spinlock_t		event_lock = SPIN_LOCK_UNLOCKED;
static int			event_is_open = 0;

static struct list_head		event_list = LIST_HEAD_INIT(event_list);

DECLARE_WAIT_QUEUE_HEAD(event_queue);

static struct battery_info batt_info;
static int ac_online; /* indicates whether AC connected */

/*
 * Begin battery and ac adapter emulation functions.
 */

static void init_battery_data(void)
{
	/*
	 * This is data from a random DELL laptop
	 */
	batt_info.present = 1; /* true/false */
	batt_info.technology = BATT_TECHNOLOGY_RECHARGEABLE;
	batt_info.capacity_state = CAPACITY_STATE_OK;
	batt_info.charging_state = CHARGING_STATE_UNKNOWN;
	batt_info.charging_rate = 0; /* in mW*/
	batt_info.design_capacity = 47520; /* in mWh */
	batt_info.design_capacity_low = 200; /* in mWh */
	batt_info.design_capacity_warning = 2343; /* in mWh */
	batt_info.last_full_capacity = 46870; /* in mWh */
	batt_info.present_capacity = 46360; /* in mwh*/
	batt_info.alarm_capacity = 2343; /* in mWh */
	batt_info.dead_capacity = 200; /* in mWh */
	batt_info.present_voltage = 12408; /* in mV */
	batt_info.design_voltage = 10800; /* in mV */
	strcpy(batt_info.model_number, "NONAME-A1good"); /* 20 chars max */
	strcpy(batt_info.serial_number, "12345678"); /* 20 chars max */
	strcpy(batt_info.type, "LION"); /* 20 chars max */
	strcpy(batt_info.manufacturer, "BATTman"); /* 20 chars max */
}

/*
 * End battery and ac adapter emulation functions.
 */

/*
 * Various utilities for handling queued events
 */
static int
enqueue_event(acpi_device_class device_class, acpi_bus_id bus_id, u32 type, u32 data)
{
	struct event_info *pei = NULL;
	u32 flags = 0;

	/* drop event on the floor if no one's listening */
	if (!event_is_open)
		return(0);

	pei = kmalloc(sizeof(struct event_info), GFP_ATOMIC);
	if (!pei)
		return(-ENOMEM);

	strncpy(pei->device_class, device_class, sizeof(acpi_device_class));
	strncpy(pei->bus_id, bus_id, sizeof(acpi_bus_id));
	pei->type = type;
	pei->data = data;

	spin_lock_irqsave(&event_lock, flags);
	list_add_tail(&pei->list_head, &event_list);
	spin_unlock_irqrestore(&event_lock, flags);

	wake_up_interruptible(&event_queue);

	return(0);
}

static int
block_and_dequeue_event(struct event_info *pevent_info)
{
	struct event_info *pei = NULL;
	u32 flags = 0;

	DECLARE_WAITQUEUE(wait, current);
	if (!pevent_info)
		return -EINVAL;

	if (list_empty(&event_list)) {

		set_current_state(TASK_INTERRUPTIBLE);
		add_wait_queue(&event_queue, &wait);

		if (list_empty(&event_list))
			schedule();

		remove_wait_queue(&event_queue, &wait);
		set_current_state(TASK_RUNNING);

		if (signal_pending(current))
			return(-ERESTARTSYS);
	}

	spin_lock_irqsave(&event_lock, flags);
	pei = list_entry(event_list.next, struct event_info, list_head);
	if (pei)
		list_del(&pei->list_head);
	spin_unlock_irqrestore(&event_lock, flags);

	if (!pei)
		return(-ENODEV);

	memcpy(pevent_info, pei, sizeof(struct event_info));

	kfree(pei);

	return(0);

}


/*
 * read and write functions for acpi proc entries
 */

static int
proc_read_info(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	/*
	 * /proc/acpi/info
	 * $ cat info
	 * version:                 20040326
	 */
	int len;

	MOD_INC_USE_COUNT;
	
	len = sprintf(page, "version:\t\t\t%s\n", __DATE__);

	MOD_DEC_USE_COUNT;

	return len;
}

static int
proc_open_event(struct inode *inode, struct file *file)
{
	spin_lock_irq (&event_lock);

	if(event_is_open)
		goto out_busy;

	MOD_INC_USE_COUNT;
	event_is_open = 1;

	spin_unlock_irq (&event_lock);
	return 0;

out_busy:
	spin_unlock_irq (&event_lock);
	return -EBUSY;
}

static ssize_t
proc_read_event (struct file *file, char *buffer, size_t count, loff_t *ppos)
{
	int result = 0;
	struct event_info event;
	static char str[ACPI_MAX_STRING];
	static int chars_remaining = 0;
	static char *ptr;

	if (!chars_remaining) {
		spin_lock_irq (&event_lock);
		if ((file->f_flags & O_NONBLOCK) && (list_empty(&event_list))) {
			spin_unlock_irq (&event_lock);
			return -EAGAIN;
		}
		spin_unlock_irq (&event_lock);

		/* Block waiting for event. */
		result = block_and_dequeue_event(&event);
		if (result) {
			return -EIO ;
		}

		chars_remaining = sprintf(str, "%s %s %08x %08x\n", 
					  event.device_class?event.device_class:"<unknown>",
					  event.bus_id?event.bus_id:"<unknown>", 
					  event.type, event.data);
		ptr = str;
	}

	if (chars_remaining < count) {
		count = chars_remaining;
	}

	if (copy_to_user(buffer, ptr, count))
		return -EFAULT ;

	*ppos += count;
	chars_remaining -= count;
	ptr += count;

	return count;
}

static int
proc_close_event(struct inode *inode, struct file *file)
{
	spin_lock_irq (&event_lock);
	event_is_open = 0;
	MOD_DEC_USE_COUNT;
	spin_unlock_irq (&event_lock);
	return 0;
}

static unsigned int
proc_poll_event(struct file *file, poll_table *wait)
{
	poll_wait(file, &event_queue, wait);
	if (!list_empty(&event_list))
		return POLLIN | POLLRDNORM;
	return 0;
}

static int
proc_read_cpu_info(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	/*
	 * /proc/acpi/CPU]/info
	 *  READ ONLY
	 * $ cat processor/CPU/info
	 * processor id:            0
	 * acpi id:                 1
	 * bus mastering control:   yes
	 * power management:        yes
	 * throttling control:      yes
	 * limit interface:         yes
	 * TODO
	 */
	int len;

	MOD_INC_USE_COUNT;
	
	len = sprintf(page, "This is the cpu/info file \n");

	MOD_DEC_USE_COUNT;

	return len;
}

static int
proc_read_cpu_limit(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	/*
	 * /proc/acpi/CPU/limit
	 *  READ
	 * $ cat processor/CPU/limit
	 * active limit:            P0:T0
	 * user limit:              P0:T0
	 * thermal limit:           P0:T0
	 * TODO
	 */

	int len;

	MOD_INC_USE_COUNT;
	
	len = sprintf(page, "This is the cpu/limit file\n");

	MOD_DEC_USE_COUNT;

	return len;
}

static int
proc_write_cpu_limit(struct file *file, const char *buffer, unsigned long count, void *data)
{
	/*
	 * /proc/acpi/CPU]/limit
	 *  WRITE
	 *    echo -n x:y > limit
	 * TODO
	 */
	int len;
	char inbuf[CPU_LEN];

	MOD_INC_USE_COUNT;

	if(count >= CPU_LEN)
		len = CPU_LEN - 1;
	else
		len = count;

	if(copy_from_user(inbuf, buffer, len)) {
		MOD_DEC_USE_COUNT;
		return -EFAULT;
	}

	inbuf[len] = '\0';
	/* parse the input and do something... */

	MOD_DEC_USE_COUNT;

	return len;
}

static int
proc_read_cpu_performance(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	/*
	 * TODO - what is this - not present on Clark's laptop?
	 * /proc/acpi/CPU]/performance
	 *  READ
	 *    "<not available>"
	 *  OR
	 *    state count:   2
	 *    active state:  P0
	 *    states:
	 *      *P0:  600 Mhz, 20000 mW, 300 uS
	 *       P1:  500 Mhz, 10000 mW, 300 uS
	 *  WRITE
	 *    echo -n x > performance
	 * TODO
	 */
	int len;

	MOD_INC_USE_COUNT;
	
	len = sprintf(page, "This is the cpu/performance file\n");

	MOD_DEC_USE_COUNT;

	return len;
}

static int
proc_write_cpu_performance(struct file *file, const char *buffer, unsigned long count, void *data)
{
	/*
	 * /proc/acpi/CPU]/performance
	 *  WRITE (if available)
	 *    echo -n x > performance
	 * TODO
	 */
	int len;
	char inbuf[CPU_LEN];

	MOD_INC_USE_COUNT;

	if(count >= CPU_LEN)
		len = CPU_LEN - 1;
	else
		len = count;

	if(copy_from_user(inbuf, buffer, len)) {
		MOD_DEC_USE_COUNT;
		return -EFAULT;
	}

	inbuf[len] = '\0';
	/* parse the input and do something... */

	MOD_DEC_USE_COUNT;

	return len;
}

/* Create CPU/power entry RO */
static int
proc_read_cpu_power(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	/*
	 * /proc/acpi/CPU/power
	 *  READ ONLY
	 * $ cat processor/CPU/power
	 * active state:            C2
	 * default state:           C1
	 * bus master activity:     ffffffff
	 * states:
	 *     C1:                  promotion[C2] demotion[--] latency[000] usage[00000010]
	 *    *C2:                  promotion[C3] demotion[C1] latency[001] usage[14096383]
	 *     C3:                  promotion[--] demotion[C2] latency[085] usage[00000997]
	 * 
	 * TODO
	 */
	int len;

	MOD_INC_USE_COUNT;
	
	len = sprintf(page, "This is the cpu/power file\n");

	MOD_DEC_USE_COUNT;

	return len;
}

static int
proc_read_cpu_throttling(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	/*
	 * /proc/acpi/CPU]/throttling
	 *  READ
	 * $ cat processor/CPU/throttling
	 * state count:             8
	 * active state:            T0
	 * states:
	 *    *T0:                  00%
	 *     T1:                  12%
	 *     T2:                  25%
	 *     T3:                  37%
	 *     T4:                  50%
	 *     T5:                  62%
	 *     T6:                  75%
	 *     T7:                  87%
	 * TODO
	 */
	int len;

	MOD_INC_USE_COUNT;
	
	len = sprintf(page, "This is the cpu/throttling file\n");

	MOD_DEC_USE_COUNT;

	return len;
}

static int
proc_write_cpu_throttling(struct file *file, const char *buffer, unsigned long count, void *data)
{
	/*
	 * /proc/acpi/CPU]/throttling
	 *  WRITE
	 *    echo -n x > throttling
	 * TODO
	 */
	int len;
	char inbuf[CPU_LEN];

	MOD_INC_USE_COUNT;

	if(count >= CPU_LEN)
		len = CPU_LEN - 1;
	else
		len = count;

	if(copy_from_user(inbuf, buffer, len)) {
		MOD_DEC_USE_COUNT;
		return -EFAULT;
	}

	inbuf[len] = '\0';
	/* parse the input and do something... */

	MOD_DEC_USE_COUNT;

	return len;
}

static int
proc_read_ac_adapter_state(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	int len;

	MOD_INC_USE_COUNT;

	if (ac_online)
		len = sprintf(page, "state:\t\t\ton-line\n");
	else
		len = sprintf(page, "state:\t\t\toff-line\n");

	MOD_DEC_USE_COUNT;

	return len;
}

static int
proc_read_BAT0_state(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	int len;

	MOD_INC_USE_COUNT;

	if (!batt_info.present) {
		/* no battery */
		len = sprintf(page, "present:\t\tno\n");
	} else {
		/* yes battery */
		len = sprintf(page, "present:\t\tyes\n");

		/* capacity state */
		len += sprintf(page+len, "capacity state:\t\t");
		switch(batt_info.capacity_state) {
		case CAPACITY_STATE_UNKNOWN:
			len += sprintf(page+len, "unknown\n");
			break;
		case CAPACITY_STATE_OK:
			len += sprintf(page+len, "ok\n");
			break;
		case CAPACITY_STATE_NOTOK:
			len += sprintf(page+len, "not ok\n");
			break;
		default:
			len += sprintf(page+len, "invalid state\n");
			break;
		}

		/* charging state */
		len += sprintf(page+len, "charging state:\t\t");
		switch(batt_info.charging_state) {
		case CHARGING_STATE_UNKNOWN:
			len += sprintf(page+len, "unknown\n");
			break;
		case CHARGING_STATE_CHARGING:
			len += sprintf(page+len, "charging\n");
			break;
		default:
			len += sprintf(page+len, "invalid state\n");
			break;
		}

		len += sprintf(page+len, "present rate:\t\t%d mW\n", batt_info.charging_rate);
		len += sprintf(page+len, "remaining capacity:\t%d mWh\n", batt_info.present_capacity);
		len += sprintf(page+len, "present voltage:\t%d mV\n", batt_info.present_voltage);
	}

	MOD_DEC_USE_COUNT;

	return len;
}

static int
proc_read_BAT0_info(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	int len;

	MOD_INC_USE_COUNT;
	
	if (!batt_info.present) {
		/* no battery */
		len = sprintf(page, "present:\t\t\tno\n");
	} else {
		/* yes battery */
		len = sprintf(page, "present:\t\t\tyes\n");

		len += sprintf(page+len, "design capacity:\t\t%d mWh\n", batt_info.design_capacity);
		len += sprintf(page+len, "last full capacity:\t\t%d mWh\n", batt_info.last_full_capacity);
		len += sprintf(page+len, "battery technology:\t\t");
		switch (batt_info.technology) {
		case BATT_TECHNOLOGY_UNKNOWN:
			len += sprintf(page+len, "unknown\n");
			break;
		case BATT_TECHNOLOGY_RECHARGEABLE:
			len += sprintf(page+len, "rechargeable\n");
			break;
		default:
			len += sprintf(page+len, "invalid value\n");
			break;
		}
		len += sprintf(page+len, "design voltage:\t\t\t%d mV\n", batt_info.design_voltage);
		len += sprintf(page+len, "design capacity warning:\t%d mWh\n", batt_info.design_capacity_warning);
		len += sprintf(page+len, "design capacity low:\t\t%d mWh\n", batt_info.design_capacity_low);
		len += sprintf(page+len, "model number:\t\t\t%s\n", batt_info.model_number);
		len += sprintf(page+len, "serial number:\t\t\t%s\n", batt_info.serial_number);
		len += sprintf(page+len, "battery type:\t\t\t%s\n", batt_info.type);
		len += sprintf(page+len, "OEM info:\t\t\t%s\n", batt_info.manufacturer);
	}

	MOD_DEC_USE_COUNT;

	return len;
}

static int
proc_read_BAT0_alarm(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	int len;

	MOD_INC_USE_COUNT;
	
	len = sprintf(page, "alarm:\t\t\t%d mWh\n", batt_info.alarm_capacity);

	MOD_DEC_USE_COUNT;

	return len;
}

static int
proc_write_BAT0_alarm(struct file *file, const char *buffer, unsigned long count, void *data)
{
	int len, tmpint;
	char inbuf[EMU_LEN];

	MOD_INC_USE_COUNT;

	if(count >= EMU_LEN)
		len = EMU_LEN - 1;
	else
		len = count;

	if(copy_from_user(inbuf, buffer, len)) {
		MOD_DEC_USE_COUNT;
		return -EFAULT;
	}

	inbuf[len] = '\0';
	tmpint = simple_strtoul(inbuf, NULL, 10);
	if (tmpint < 0)
		tmpint = 0;
	else if (tmpint > batt_info.design_capacity)
		tmpint = batt_info.design_capacity;
	batt_info.alarm_capacity = tmpint;

	MOD_DEC_USE_COUNT;

	return len;
}

static int
proc_read_BAT0_dead(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	int len;

	MOD_INC_USE_COUNT;
	
	len = sprintf(page, "alarm:\t\t\t%d mWh\n", batt_info.dead_capacity);

	MOD_DEC_USE_COUNT;

	return len;
}

static int
proc_write_BAT0_dead(struct file *file, const char *buffer, unsigned long count, void *data)
{
	int len, tmpint;
	char inbuf[EMU_LEN];

	MOD_INC_USE_COUNT;

	if(count >= EMU_LEN)
		len = EMU_LEN - 1;
	else
		len = count;

	if(copy_from_user(inbuf, buffer, len)) {
		MOD_DEC_USE_COUNT;
		return -EFAULT;
	}

	inbuf[len] = '\0';
	tmpint = simple_strtoul(inbuf, NULL, 10);
	if (tmpint < 0)
		tmpint = 0;
	else if (tmpint > batt_info.design_capacity)
		tmpint = batt_info.design_capacity;
	batt_info.dead_capacity = tmpint;

	MOD_DEC_USE_COUNT;

	return len;
}

static int
proc_read_BAT0_emulatecapacity(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	int len;

	MOD_INC_USE_COUNT;
	
	len = sprintf(page, "capacity:\t\t\t%d mWh\n", batt_info.present_capacity);

	MOD_DEC_USE_COUNT;

	return len;
}

static int
proc_write_BAT0_emulatecapacity(struct file *file, const char *buffer, unsigned long count, void *data)
{
	int len, tmpint;
	char inbuf[EMU_LEN];

	MOD_INC_USE_COUNT;

	if(count >= EMU_LEN)
		len = EMU_LEN - 1;
	else
		len = count;

	if(copy_from_user(inbuf, buffer, len)) {
		MOD_DEC_USE_COUNT;
		return -EFAULT;
	}

	inbuf[len] = '\0';
	tmpint = simple_strtoul(inbuf, NULL, 10);
	if (tmpint < 0)
		tmpint = 0;
	else if (tmpint > batt_info.design_capacity)
		tmpint = batt_info.design_capacity;
	batt_info.present_capacity = tmpint;

	MOD_DEC_USE_COUNT;

	return len;
}

static int
proc_read_button_power_info(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	/*
	 * $ cat button/power/PWRF/info
	 * type:                    Power Button (FF)
	 * TODO
	 */
	int len;

	MOD_INC_USE_COUNT;
	
	len = sprintf(page, "type:\tPower Button (FF)\n");

	MOD_DEC_USE_COUNT;

	return len;
}

#ifdef DEBUG_PWRF

static u32 button_power_state_write_count = 0;

/* enable write to the power button state, for testing */
static int
proc_write_button_power_state(struct file *file, const char *buffer, unsigned long count, void *data)
{
	unsigned long value;
	char inbuf[EMU_LEN];

	int len = count;

	MOD_INC_USE_COUNT;

	if (count >= EMU_LEN)
		len = EMU_LEN - 1;

	if(copy_from_user(inbuf, buffer, len)) {
		MOD_DEC_USE_COUNT;
		return -EFAULT;
	}

	inbuf[len] = '\0';
	value = simple_strtol(inbuf, NULL, 10);
	if (value) {
		if (enqueue_event("button/power", "PWRF", ACPI_BUTTON_NOTIFY_STATUS, button_power_state_write_count++)) {
			printk(KERN_ERR "facpi: Error enqueuing power button event\n");
		}
	}

	MOD_DEC_USE_COUNT;

	return len;
}
#endif

static int
proc_read_button_power_state(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	/*
	 * $ cat button/power/PWRF/state
	 * xxxxx
	 * TODO
	 */
	int len;

	MOD_INC_USE_COUNT;
	
	len = sprintf(page, "This is the button/power state file\n");

	MOD_DEC_USE_COUNT;

	return len;
}

static unsigned long suspend_reload = 0;
static unsigned long standby_reload = 0;
static unsigned long suspend_timeout = 0;
static unsigned long standby_timeout = 0;

static int
proc_read_bios_suspend_timeout(char *page, char**start, off_t off, int count, int *eof, void *data)
{
	MOD_INC_USE_COUNT;

	count = sprintf(page, "%lu\n", suspend_timeout);

	MOD_DEC_USE_COUNT;
	return count;
}

static int
proc_read_bios_standby_timeout(char *page, char**start, off_t off, int count, int *eof, void *data)
{
	MOD_INC_USE_COUNT;

	count = sprintf(page, "%lu\n", standby_timeout);

	MOD_DEC_USE_COUNT;
	return count;
}

static struct timer_list suspend_timer;

static void
suspend_timer_handler(unsigned long data)
{
	if (!suspend_timeout) {
		/* suspend counted to zero last time through */
		suspend_timeout = suspend_reload;
	} else {
		/* decrement the suspend counter */
		if (!--suspend_timeout) {
			/* if it just went to zero, queue an event */
			enqueue_event("timer/suspend", "SUSP",
				      ACPI_BUTTON_NOTIFY_STATUS, 0);
		}
	}
	if (!standby_timeout) {
		standby_timeout = standby_reload;
	} else {
		if (!--standby_timeout) {
			enqueue_event("timer/standby", "STND",
				      ACPI_BUTTON_NOTIFY_STATUS, 0);
		}
	}
	/*
	 * If either suspend or standby timeouts are active,
	 * restart the timer.
	 */
	if (suspend_reload | standby_reload) {
		suspend_timer.expires = jiffies + HZ;
		add_timer(&suspend_timer);
	} else
		MOD_DEC_USE_COUNT;
}

static int
proc_write_bios_suspend_timeout(struct file *file, const char *buffer, unsigned long count, void *data)
{
	char inbuf[EMU_LEN];

	int len = count;

	MOD_INC_USE_COUNT;

	if (count >= EMU_LEN)
		len = EMU_LEN - 1;

	if(copy_from_user(inbuf, buffer, len)) {
		MOD_DEC_USE_COUNT;
		return -EFAULT;
	}

	inbuf[len] = '\0';
	suspend_reload = simple_strtol(inbuf, NULL, 10);
	suspend_timeout = suspend_reload;

	if (timer_pending(&suspend_timer)) {
		/* the timer is running. stop it if we don't need it */
		if (!(suspend_reload | standby_reload)) {
			del_timer(&suspend_timer);
			MOD_DEC_USE_COUNT;
		}
	} else {
		/* the timer is not running. start it if we need it */
		if (suspend_reload) {
			suspend_timer.expires = jiffies + HZ;
			add_timer(&suspend_timer);
			MOD_INC_USE_COUNT;
		}
	}

	MOD_DEC_USE_COUNT;

	return len;
}

static int
proc_write_bios_standby_timeout(struct file *file, const char *buffer, unsigned long count, void *data)
{
	char inbuf[EMU_LEN];

	int len = count;

	MOD_INC_USE_COUNT;

	if (count >= EMU_LEN)
		len = EMU_LEN - 1;

	if(copy_from_user(inbuf, buffer, len)) {
		MOD_DEC_USE_COUNT;
		return -EFAULT;
	}

	inbuf[len] = '\0';
	standby_reload = simple_strtol(inbuf, NULL, 10);
	standby_timeout = standby_reload;

	if (timer_pending(&suspend_timer)) {
		if (!(standby_reload | suspend_reload)) {
			del_timer(&suspend_timer);
			MOD_DEC_USE_COUNT;
		}
	} else {
		if (standby_reload) {
			suspend_timer.expires = jiffies + HZ;
			add_timer(&suspend_timer);
			MOD_INC_USE_COUNT;
		}
	}

	MOD_DEC_USE_COUNT;

	return len;
}

static int
proc_write_bios_restart_timeout(struct file *file, const char *buffer, unsigned long count, void *data)
{
	unsigned long value;
	char inbuf[EMU_LEN];

	int len = count;

	MOD_INC_USE_COUNT;

	if (count >= EMU_LEN)
		len = EMU_LEN - 1;

	if(copy_from_user(inbuf, buffer, len)) {
		MOD_DEC_USE_COUNT;
		return -EFAULT;
	}

	inbuf[len] = '\0';
	value = simple_strtol(inbuf, NULL, 10);
	if (value) {
		if (del_timer(&suspend_timer))
			MOD_DEC_USE_COUNT;
		suspend_timeout = suspend_reload;
		standby_timeout = standby_reload;
		if (standby_timeout | suspend_timeout) {
			suspend_timer.expires = jiffies + HZ;
			add_timer(&suspend_timer);
			MOD_INC_USE_COUNT;
		}
	}

	MOD_DEC_USE_COUNT;

	return len;
}

enum powerdown_mode {
	powerdown_shutdown,
	powerdown_suspend,
};

static enum powerdown_mode current_powerdown_mode = powerdown_suspend;

static int
proc_read_bios_powerdown_mode(char *page, char**start, off_t off, int count, int *eof, void *data)
{
	MOD_INC_USE_COUNT;

	switch (current_powerdown_mode){
	case powerdown_suspend:
		strcpy(page, "suspend\n");
		break;
	case powerdown_shutdown:
		strcpy(page, "shutdown\n");
		break;
	}

	MOD_DEC_USE_COUNT;

	return strlen(page);
}

static int
proc_write_bios_powerdown_mode(struct file *file, const char *buffer, unsigned long count, void *data)
{
	char inbuf[EMU_LEN], *p;

	int len = count;

	MOD_INC_USE_COUNT;

	if (count >= EMU_LEN)
		len = EMU_LEN - 1;

	if(copy_from_user(inbuf, buffer, len)) {
		MOD_DEC_USE_COUNT;
		return -EFAULT;
	}

	inbuf[len] = '\0';

	/* find the first non-space character */
	p = inbuf;
	while (len && isspace(*p)) {
		p++;
		len--;
	}
	/* shorten the length to the last non-space character */
	while (len && isspace(p[len-1]))
		len--;
	p[len] = '\0';

	if (len == strlen("suspend") && strnicmp(p, "suspend", len) == 0)
		current_powerdown_mode = powerdown_suspend;
	else if (len == strlen("shutdown") && strnicmp(p, "shutdown", len) == 0)
		current_powerdown_mode = powerdown_shutdown;

	MOD_DEC_USE_COUNT;

	return count;
}

long powersave_mode;

static int
proc_read_bios_powersave_mode(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	int len;

	MOD_INC_USE_COUNT;
	
	len = sprintf(page, "%ld\n", powersave_mode);

	MOD_DEC_USE_COUNT;

	return len;
}

static int
proc_write_bios_powersave_mode(struct file *file, const char *buffer, unsigned long count, void *data)
{
	int len;
	char inbuf[EMU_LEN];

	MOD_INC_USE_COUNT;

	if(count >= EMU_LEN)
		len = EMU_LEN - 1;
	else
		len = count;

	if(copy_from_user(inbuf, buffer, len)) {
		MOD_DEC_USE_COUNT;
		return -EFAULT;
	}

	inbuf[len] = '\0';
	powersave_mode = simple_strtol(inbuf, NULL, 10);
	if (powersave_mode)
		powersave_mode = 1;

	MOD_DEC_USE_COUNT;

	return len;
}


/*
 *
 * Kernel thread
 *
 */

static struct task_struct *kacpid_task;

static int
kacpid(void *__unused)
{
	int batt_alarm_generated = 0;
	int batt_dead_generated = 0;

	daemonize();
	strcpy(current->comm, "kacpid");
	kacpid_task = current;

	printk(KERN_INFO "facpi: kacpid starting...\n");
	for (;;) {

		current->state = TASK_INTERRUPTIBLE;
		schedule_timeout(POLL_INTERVAL);
		current->state = TASK_RUNNING;
		if (signal_pending(current))
			break;
		/* Add our processing here
		 *  TODO
		 * Check AC power */
		ac_online = 1; /* TODO how to check this really? */

		/* Check emulated battery */
		if (!batt_alarm_generated) {
			if (batt_info.present_capacity < batt_info.alarm_capacity) {
				/* generate an event - battery is below capacity */
				batt_alarm_generated = 1;
				if (enqueue_event("battery low", "0", 0, 0)) {
					printk(KERN_ERR "facpi: Error enqueuing battery alarm event\n");
				}
			}
		}
		else {
			/* test for return to non-alarmed condition */
			if (batt_info.present_capacity >= batt_info.alarm_capacity) {
				batt_alarm_generated = 0;
			}
		}
		if (!batt_dead_generated) {
			if (batt_info.present_capacity < batt_info.dead_capacity) {
				/* generate an event - battery is dead */
				batt_dead_generated = 1;
				if (enqueue_event("battery dead", "0", 0, 0)) {
					printk(KERN_ERR "facpi: Error enqueuing battery dead event\n");
				}
			}
		}
		else {
			/* test for return to non-dead condition */
			if (batt_info.present_capacity >= batt_info.dead_capacity) {
				batt_dead_generated = 0;
			}
		}
	}
	printk(KERN_INFO "facpi: kacpid exiting...\n");

	return 0;
}

/*
 * =============================================================
 */

#ifdef CONFIG_MB93093_PDK

/* PDK user button handler */

static u32 user_button4_press_count = 0;

static struct timer_list debounce_timer;

static void
debounce_timeout(unsigned long data)
{
	u32 swmask = data;

	/* if a button is still pressed, keep waiting */
	if (__get_FPGA_PUSHSW1_5() != MB93093_FPGA_SWR_PUSHSWMASK) {
		debounce_timer.expires = jiffies + 10;
		add_timer(&debounce_timer);
		return;
	}

	/* do something for button 4 */
	if ((swmask & MB93093_FPGA_SWR_PUSHSW4) == 0) {
		if (current_powerdown_mode == powerdown_suspend) {
			if (enqueue_event("button/sleep", "SLPB", ACPI_BUTTON_NOTIFY_STATUS, user_button4_press_count++)) {
				printk(KERN_ERR "facpi: Error enqueuing power button event\n");
			}
		} else
			enqueue_event("button/power", "PWRF", ACPI_BUTTON_NOTIFY_STATUS, user_button4_press_count++);
	}

	enable_irq(IRQ_FPGA_PUSH_BUTTON_SW1_5);

	MOD_DEC_USE_COUNT;
}

static void
first_debounce_timeout(unsigned long data)
{
	disable_irq(IRQ_FPGA_PUSH_BUTTON_SW1_5);
	debounce_timer.function = debounce_timeout;
	debounce_timeout(data);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
typedef void irqreturn_t;
#endif

/* this gets the interrupt for *all* the pushbuttons */
irqreturn_t handle_user_button(int irq, void *dev_id, struct pt_regs *regs)
{
	MOD_INC_USE_COUNT;

	/* hang on to the current bushbutton mask (zero = pressed) */
	u32 swmask = __get_FPGA_PUSHSW1_5();

	/* debounce the button */
	if (!timer_pending(&debounce_timer)) {
		debounce_timer.function = first_debounce_timeout;
		debounce_timer.expires = jiffies + 1;
		debounce_timer.data = swmask;
		add_timer(&debounce_timer);
	}
	else
		MOD_DEC_USE_COUNT;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
	return;
#else
	return IRQ_HANDLED;
#endif
}

#endif

/*
 * Module Initialization
 */

static int __init facpi_init_module(void) 
{

	printk("Fujitsu FRV ACPI driver. Built: %s\n", __DATE__);

	int rv = 0;
	int err;

#ifdef CONFIG_MB93093_PDK
	init_timer(&debounce_timer);
#endif

	init_battery_data();

	/* create top directory */
	top_dir = proc_mkdir(TOPDIR_NAME, NULL);
	if(top_dir == NULL) {
		rv = -ENOMEM;
		goto out;
	}
	
	top_dir->owner = THIS_MODULE;
	
	/* create info directory */
	info_file = create_proc_read_entry("info", 0444, top_dir, proc_read_info, NULL);

	if(info_file == NULL) {
		rv = -ENOMEM;
		goto no_info;
	}

	info_file->owner = THIS_MODULE;


	/* create event directory */
static struct file_operations event_ops = {
	.open =		proc_open_event,
	.read =		proc_read_event,
	.release =	proc_close_event,
	.poll =		proc_poll_event,
};


	event_file = create_proc_entry("event",	0444, top_dir);
	if (event_file == NULL) {
		rv = -ENOMEM;
		goto no_event;
	}
	event_file->proc_fops = &event_ops;
	event_file->owner = THIS_MODULE;

	/* Create CPU top directory */
	CPU_dir = proc_mkdir("CPU", top_dir);
	if(CPU_dir == NULL) {
		rv = -ENOMEM;
		goto no_CPU;
	}
	
	CPU_dir->owner = THIS_MODULE;

	/* Create CPU/info entry RO */
	CPU_info_file = create_proc_read_entry("info", 0444, CPU_dir, proc_read_cpu_info, NULL);

	if(CPU_info_file == NULL) {
		rv = -ENOMEM;
		goto no_CPU_info;
	}

	CPU_info_file->owner = THIS_MODULE;

	/* Create CPU/limit entry */
	CPU_limit_file = create_proc_entry("limit", 0644, CPU_dir);

	if(CPU_limit_file == NULL) {
		rv = -ENOMEM;
		goto no_CPU_limit;
	}

	CPU_limit_file->read_proc = proc_read_cpu_limit;
	CPU_limit_file->write_proc = proc_write_cpu_limit;
	CPU_limit_file->owner = THIS_MODULE;

	/* Create CPU/performance entry */
	CPU_performance_file = create_proc_entry("performance", 0644, CPU_dir);

	if(CPU_performance_file == NULL) {
		rv = -ENOMEM;
		goto no_CPU_performance;
	}

	CPU_performance_file->read_proc = proc_read_cpu_performance;
	CPU_performance_file->write_proc = proc_write_cpu_performance;
	CPU_performance_file->owner = THIS_MODULE;

	/* Create CPU/power entry RO */
	CPU_power_file = create_proc_read_entry("power", 0444, CPU_dir, proc_read_cpu_power, NULL);

	if(CPU_power_file == NULL) {
		rv = -ENOMEM;
		goto no_CPU_power;
	}

	CPU_power_file->owner = THIS_MODULE;

	/* Create CPU/throttling entry */
	CPU_throttling_file = create_proc_entry("throttling", 0644, CPU_dir);

	if(CPU_throttling_file == NULL) {
		rv = -ENOMEM;
		goto no_CPU_throttling;
	}

	CPU_throttling_file->read_proc = proc_read_cpu_throttling;
	CPU_throttling_file->write_proc = proc_write_cpu_throttling;
	CPU_throttling_file->owner = THIS_MODULE;

	/* Create ac_adapter top directory */
	ac_adapter_dir = proc_mkdir("ac_adapter", top_dir);
	if(ac_adapter_dir == NULL) {
		rv = -ENOMEM;
		goto no_ac_adapter;
	}
	
	ac_adapter_dir->owner = THIS_MODULE;

	/* Create ac_adapter/state entry RO */
	ac_adapter_state_file = create_proc_read_entry("state", 0444, ac_adapter_dir, proc_read_ac_adapter_state, NULL);

	if(ac_adapter_state_file == NULL) {
		rv = -ENOMEM;
		goto no_ac_adapter_state;
	}

	ac_adapter_state_file->owner = THIS_MODULE;


	/* Create battery top directory */
	battery_dir = proc_mkdir("battery", top_dir);
	if(battery_dir == NULL) {
		rv = -ENOMEM;
		goto no_battery;
	}
	
	battery_dir->owner = THIS_MODULE;

	/* Create battery/BAT0 directory */
	BAT0_dir = proc_mkdir("BAT0", battery_dir);
	if(BAT0_dir == NULL) {
		rv = -ENOMEM;
		goto no_BAT0;
	}
	
	BAT0_dir->owner = THIS_MODULE;

	/* Create battery/BAT0/state entry */
	BAT0_state_file = create_proc_read_entry("state", 0444, BAT0_dir, proc_read_BAT0_state, NULL);

	if(BAT0_state_file == NULL) {
		rv = -ENOMEM;
		goto no_BAT0_state;
	}

	BAT0_state_file->owner = THIS_MODULE;

	/* Create battery/BAT0/info entry */
	BAT0_info_file = create_proc_read_entry("info", 0444, BAT0_dir, proc_read_BAT0_info, NULL);

	if(BAT0_info_file == NULL) {
		rv = -ENOMEM;
		goto no_BAT0_info;
	}

	BAT0_state_file->owner = THIS_MODULE;

	/* Create battery/BAT0/alarm entry */
	BAT0_alarm_file = create_proc_entry("alarm", 0644, BAT0_dir);

	if(BAT0_alarm_file == NULL) {
		rv = -ENOMEM;
		goto no_BAT0_alarm;
	}

	BAT0_alarm_file->owner = THIS_MODULE;
	BAT0_alarm_file->read_proc = proc_read_BAT0_alarm;
	BAT0_alarm_file->write_proc = proc_write_BAT0_alarm;

	/* Create battery/BAT0/dead entry */
	BAT0_dead_file = create_proc_entry("dead", 0644, BAT0_dir);

	if(BAT0_dead_file == NULL) {
		rv = -ENOMEM;
		goto no_BAT0_dead;
	}

	BAT0_dead_file->owner = THIS_MODULE;
	BAT0_dead_file->read_proc = proc_read_BAT0_dead;
	BAT0_dead_file->write_proc = proc_write_BAT0_dead;

	/* create the file used to emulate the battery */
	/* battery/BAT0/emulatecapacity */
	BAT0_emulatecapacity_file = create_proc_entry("emulatecapacity", 0644, BAT0_dir);

	if(BAT0_emulatecapacity_file == NULL) {
		rv = -ENOMEM;
		goto no_BAT0_emulatecapacity;
	}

	BAT0_emulatecapacity_file->read_proc = proc_read_BAT0_emulatecapacity;
	BAT0_emulatecapacity_file->write_proc = proc_write_BAT0_emulatecapacity;
	BAT0_emulatecapacity_file->owner = THIS_MODULE;


	/* Create button top directory */
	button_dir = proc_mkdir("button", top_dir);
	if(button_dir == NULL) {
		rv = -ENOMEM;
		goto no_button;
	}
	
	button_dir->owner = THIS_MODULE;

	/* Create button/power directory */
	button_power_dir = proc_mkdir("power", button_dir);
	if(button_power_dir == NULL) {
		rv = -ENOMEM;
		goto no_button_power;
	}
	
	button_power_dir->owner = THIS_MODULE;

	/* Create button/power/PWRF directory */
	button_power_PWRF_dir = proc_mkdir("PWRF", button_power_dir);
	if(button_power_PWRF_dir == NULL) {
		rv = -ENOMEM;
		goto no_button_power_PWRF;
	}
	
	button_power_PWRF_dir->owner = THIS_MODULE;

	/* Create button/power/PWRF/info entry */
	button_power_info_file =
		create_proc_read_entry("info", 0444, button_power_PWRF_dir,
				       proc_read_button_power_info, NULL);

	if(button_power_info_file == NULL) {
		rv = -ENOMEM;
		goto no_button_power_PWRF_info;
	}

	button_power_info_file->owner = THIS_MODULE;
#ifndef DEBUG_PWRF
	/* Create button/power/PWRF/state entry */
	button_power_state_file =
		create_proc_read_entry("state", 0444, button_power_PWRF_dir,
				       proc_read_button_power_state, NULL);
#else
	button_power_state_file = create_proc_entry("state", 0644, button_power_PWRF_dir);
#endif
	if(button_power_state_file == NULL) {
		rv = -ENOMEM;
		goto no_button_power_PWRF_state;
	}
#ifdef DEBUG_PWRF
	button_power_state_file->read_proc = proc_read_button_power_state;
	button_power_state_file->write_proc = proc_write_button_power_state;
#endif

	button_power_state_file->owner = THIS_MODULE;

	/* emulation for thiings that are normally set in a PC BIOS */
	bios_dir = proc_mkdir("bios_emulation", top_dir);
	if (!bios_dir) {
		rv = -ENOMEM;
		goto no_bios_dir;
	}
	bios_dir->owner = THIS_MODULE;

	bios_suspend_timeout_file = create_proc_entry("suspend_timeout", 0644, bios_dir);
	if (!bios_suspend_timeout_file) {
		rv = -ENOMEM;
		goto no_bios_suspend_timeout;
	}
	bios_suspend_timeout_file->owner = THIS_MODULE;
	bios_suspend_timeout_file->read_proc = proc_read_bios_suspend_timeout;
	bios_suspend_timeout_file->write_proc = proc_write_bios_suspend_timeout;

	bios_standby_timeout_file = create_proc_entry("standby_timeout", 0644, bios_dir);
	if (!bios_standby_timeout_file) {
		rv = -ENOMEM;
		goto no_bios_standby_timeout;
	}
	bios_standby_timeout_file->owner = THIS_MODULE;
	bios_standby_timeout_file->read_proc = proc_read_bios_standby_timeout;
	bios_standby_timeout_file->write_proc = proc_write_bios_standby_timeout;

	bios_restart_timeout_file = create_proc_entry("restart_timeout", 0444, bios_dir);
	if (!bios_restart_timeout_file) {
		rv = -ENOMEM;
		goto no_bios_restart_timeout;
	}
	bios_restart_timeout_file->owner = THIS_MODULE;
	bios_restart_timeout_file->write_proc = proc_write_bios_restart_timeout;

	init_timer(&suspend_timer);
	suspend_timer.function = suspend_timer_handler;

	bios_powerdown_mode_file = create_proc_entry("powerdown_mode", 0644, bios_dir);
	if (!bios_powerdown_mode_file) {
		rv = -ENOMEM;
		goto no_bios_powerdown_mode;
	}
	bios_powerdown_mode_file->owner = THIS_MODULE;
	bios_powerdown_mode_file->read_proc = proc_read_bios_powerdown_mode;
	bios_powerdown_mode_file->write_proc = proc_write_bios_powerdown_mode;

	bios_powersave_mode_file = create_proc_entry("powersave_mode", 0644, bios_dir);
	if (!bios_powersave_mode_file) {
		rv = -ENOMEM;
		goto no_bios_powersave_mode;
	}
	bios_powersave_mode_file->owner = THIS_MODULE;
	bios_powersave_mode_file->read_proc = proc_read_bios_powersave_mode;
	bios_powersave_mode_file->write_proc = proc_write_bios_powersave_mode;

#ifdef CONFIG_MB93093_PDK
	rv = request_irq(IRQ_FPGA_PUSH_BUTTON_SW1_5, handle_user_button,
			 SA_INTERRUPT, "Button", NULL);
	if (rv)
		goto no_button_handler;
#endif

	/* Start the kernel thread */
	err = kernel_thread(kacpid, NULL, CLONE_FS | CLONE_FILES);

	/* We're happy */
	printk(KERN_INFO "%s initialised\n", MODULE_NAME);
	return 0;

	/* error exit code might need to free_irq(...); */
no_button_handler:
	remove_proc_entry("powersave_mode", bios_powersave_mode_file);
no_bios_powersave_mode:
	remove_proc_entry("powerdown_mode", bios_powerdown_mode_file);
no_bios_powerdown_mode:
	remove_proc_entry("restart_timeout", bios_restart_timeout_file);
no_bios_restart_timeout:
	remove_proc_entry("standby_timeout", bios_standby_timeout_file);
no_bios_standby_timeout:
	remove_proc_entry("suspend_timeout", bios_suspend_timeout_file);
no_bios_suspend_timeout:
	remove_proc_entry("bios_emulation", bios_dir);
no_bios_dir:
	remove_proc_entry("state", button_power_state_file);
no_button_power_PWRF_state:
	remove_proc_entry("info", button_power_PWRF_dir);
no_button_power_PWRF_info:
	remove_proc_entry("PWRF", button_power_dir);
no_button_power_PWRF:
	remove_proc_entry("power", button_dir);
no_button_power:
	remove_proc_entry("button", top_dir);
no_button:
	remove_proc_entry("emulatecapacity", BAT0_dir);
no_BAT0_emulatecapacity:
	remove_proc_entry("dead", BAT0_dir);
no_BAT0_dead:
	remove_proc_entry("alarm", BAT0_dir);
no_BAT0_alarm:
	remove_proc_entry("info", BAT0_dir);
no_BAT0_info:
	remove_proc_entry("state", BAT0_dir);
no_BAT0_state:
	remove_proc_entry("BAT0", battery_dir);
no_BAT0:
	remove_proc_entry("battery", top_dir);
no_battery:
	remove_proc_entry("state", ac_adapter_dir);
no_ac_adapter_state:
	remove_proc_entry("ac_adapter", top_dir);
no_ac_adapter:
	remove_proc_entry("throttling", CPU_dir);
no_CPU_throttling:
	remove_proc_entry("power", CPU_dir);
no_CPU_power:
	remove_proc_entry("performance", CPU_dir);
no_CPU_performance:
	remove_proc_entry("limit", CPU_dir);
no_CPU_limit:
	remove_proc_entry("info", CPU_dir);
no_CPU_info:
	remove_proc_entry("CPU", top_dir);
no_CPU:
	remove_proc_entry("event", top_dir);
no_event:
	remove_proc_entry("info", top_dir);
no_info:
	remove_proc_entry(TOPDIR_NAME, NULL);
out:
	return rv;
}

static void __exit facpi_cleanup_module(void) 
{
	/* Kill the kernel thread */
	if (kacpid_task != NULL) {
		force_sig(SIGKILL, kacpid_task);
		for (;;) {
			struct task_struct *p;
			int found = 0;

			read_lock(&tasklist_lock);
			for_each_process(p) {
				if (p == kacpid_task) {
					found = 1;
					break;
				}
			}
			read_unlock(&tasklist_lock);
			if (!found)
				break;
			current->state = TASK_INTERRUPTIBLE;
			schedule_timeout(HZ);
			current->state = TASK_RUNNING;
		}
		kacpid_task = NULL;
	}

#ifdef CONFIG_MB93093_PDK
	free_irq(IRQ_FPGA_PUSH_BUTTON_SW1_5, NULL);
#endif

	remove_proc_entry("bios_emulation", bios_dir);
	remove_proc_entry("info", button_power_PWRF_dir);
	remove_proc_entry("PWRF", button_power_dir);
	remove_proc_entry("power", button_dir);
	remove_proc_entry("button", top_dir);
	remove_proc_entry("emulatecapacity", BAT0_dir);
	remove_proc_entry("alarm", BAT0_dir);
	remove_proc_entry("info", BAT0_dir);
	remove_proc_entry("state", BAT0_dir);
	remove_proc_entry("BAT0", battery_dir);
	remove_proc_entry("battery", top_dir);
	remove_proc_entry("state", ac_adapter_dir);
	remove_proc_entry("ac_adapter", top_dir);
	remove_proc_entry("throttling", CPU_dir);
	remove_proc_entry("power", CPU_dir);
	remove_proc_entry("performance", CPU_dir);
	remove_proc_entry("limit", CPU_dir);
	remove_proc_entry("info", CPU_dir);
	remove_proc_entry("CPU", top_dir);
	remove_proc_entry("event", top_dir);
	remove_proc_entry("info", top_dir);
	remove_proc_entry(TOPDIR_NAME, NULL);

	printk(KERN_INFO "%s removed\n", MODULE_NAME);
}

module_init(facpi_init_module); 
module_exit(facpi_cleanup_module);
