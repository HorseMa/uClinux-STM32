/*
 * interface data access header
 *
 * $Id: interface.h,v 1.10 2004/11/16 23:42:03 rstory Exp $
 */
#ifndef NETSNMP_ACCESS_INTERFACE_CONFIG_H
#define NETSNMP_ACCESS_INTERFACE_CONFIG_H

/**---------------------------------------------------------------------*/
/*
 * configure required files
 *
 * Notes:
 *
 * 1) prefer functionality over platform, where possible. If a method
 *    is available for multiple platforms, test that first. That way
 *    when a new platform is ported, it won't need a new test here.
 *
 * 2) don't do detail requirements here. If, for example,
 *    HPUX11 had different reuirements than other HPUX, that should
 *    be handled in the *_hpux.h header file.
 */
config_require(if-mib/data_access/interface_common)
#if defined( linux )
config_require(if-mib/data_access/interface_linux);
config_require(if-mib/data_access/interface_ioctl);
#else
#   define NETSNMP_ACCESS_INTERFACE_NOARCH 1
#endif

/*
 * since the configure script will pick up this header and include it in
 * mib_module_includes.h, but actual interface structure definitions which
 * are used in other headers are defined in net-snmp/data_access/interface.h,
 * we need to ignore the normal convention of not including headers in
 * a header, or we will not be able to compile.
 */
#include <net-snmp/data_access/interface.h>

#endif /* NETSNMP_ACCESS_INTERFACE_CONFIG_H */
