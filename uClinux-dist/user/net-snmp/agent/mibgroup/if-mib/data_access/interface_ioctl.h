/*
 * ioctl interface data access header
 *
 * $Id: interface_ioctl.h,v 1.5 2004/10/18 03:49:50 rstory Exp $
 */
#ifndef NETSNMP_ACCESS_INTERFACE_IOCTL_H
#define NETSNMP_ACCESS_INTERFACE_IOCTL_H

#ifdef __cplusplus
extern          "C" {
#endif

/**---------------------------------------------------------------------*/
/**/

int
netsnmp_access_interface_ioctl_physaddr_get(int fd,
                                            netsnmp_interface_entry *ifentry);

int
netsnmp_access_interface_ioctl_flags_get(int fd,
                                         netsnmp_interface_entry *ifentry);

int
netsnmp_access_interface_ioctl_flags_set(int fd,
                                         netsnmp_interface_entry *ifentry,
                                         unsigned int flags,
                                         int and_complement);

int
netsnmp_access_interface_ioctl_mtu_get(int fd,
                                       netsnmp_interface_entry *ifentry);

oid
netsnmp_access_interface_ioctl_ifindex_get(int fd, const char *name);

/**---------------------------------------------------------------------*/

# ifdef __cplusplus
}
#endif

#endif /* NETSNMP_ACCESS_INTERFACE_IOCTL_H */
