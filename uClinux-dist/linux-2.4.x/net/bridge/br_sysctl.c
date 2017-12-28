/*
 * br_sysctl.c: sysctl interface to bridge subsystem.
 *
 * 27th January, 2006 - Added a /proc/sys entry to allow disabling of mac learning.
 */
#include <linux/mm.h>
#include <linux/sysctl.h>

int br_sysctl_disable_fdb = 0;

ctl_table br_table[] = {
	{NET_BRIDGE_FDB_DISABLE, "fdb_disable", &br_sysctl_disable_fdb, sizeof(int), 0644, NULL, &proc_dointvec},
	{0}
};
