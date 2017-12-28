/*
 * FR-V ACPI Driver
 *
 * Copyright (c) 2004 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License.
 *
 */

#ifndef FACPI_H
#define FACPI_H

/* define this to make all debugs print all the time */
/* #define KERN_DEBUG KERN_CRIT */

#if !defined(__KERNEL__) 
#error "Use kernel source code to compile this driver"
#endif

#define MODULE_NAME "facpi"
#define TOPDIR_NAME "acpi"
#define SLEEP_LEN 40 /* max length of write to sleep procfs entry */
#define CPU_LEN 40 /* max length of write to CPU procfs entry */

/* string buffer length */
#define ACPI_MAX_STRING 128

/* The poll interval of the kernel thread */
#define POLL_INTERVAL	(5 * HZ)

typedef char			acpi_bus_id[5];
typedef char			acpi_device_class[20];

/* Structure for tracking events */

struct event_info {
	struct list_head list_head;
	acpi_device_class device_class;
	acpi_bus_id bus_id;
	u32 type;
	u32 data;
};

/*
 * Battery information
 */

/* customize as needed */
#define CAPACITY_STATE_UNKNOWN 0
#define CAPACITY_STATE_OK 1
#define CAPACITY_STATE_NOTOK 2
#define CHARGING_STATE_UNKNOWN 0
#define CHARGING_STATE_CHARGING 1
#define BATT_TECHNOLOGY_UNKNOWN 0
#define BATT_TECHNOLOGY_RECHARGEABLE 1


struct battery_info
{
	int present; /* true/false */
	int technology;
	int capacity_state;
	int charging_state;
	int charging_rate; /* in mW */
	int design_capacity; /* in mWh */
	int design_capacity_low; /* in mWh */
	int design_capacity_warning; /* in mWh */
	int last_full_capacity; /* in mWh */
	int present_capacity; /* in mwh */
	int alarm_capacity; /* in mWh */
	int dead_capacity; /* in mWh */
	int present_voltage; /* in mV */
	int design_voltage; /* in mV */
	char model_number[20];
	char serial_number[20];
	char type[20];
	char manufacturer[20];
};

static int __init     facpi_init_module(void);
static void __exit    facpi_cleanup_module(void);

#endif /* FACPI_H */
