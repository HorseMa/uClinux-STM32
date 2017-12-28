/*
 * route data access header
 *
 * $Id: route.h,v 1.2 2004/09/02 00:08:30 rstory Exp $
 */
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
config_require(ip-forward-mib/data_access/route_common)
#if defined( linux )
config_require(ip-forward-mib/data_access/route_linux);
#else
#error "the route data access library is not available for this platform."
#endif

/** need interface for ifIndex */
config_require(if-mib/data_access/interface);

