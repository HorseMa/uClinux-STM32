/*
 * systemstats data access header
 *
 * $Id: systemstats.h,v 1.1 2004/07/10 18:05:44 rstory Exp $
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
config_require(ip-mib/data_access/systemstats_common);
#if defined( linux )
config_require(ip-mib/data_access/systemstats_linux);
#else
/*
 * couldn't determine the correct file!
 * require a bogus file to generate an error.
 */
configure_require(ip-mib/data_access/systemstats-unknown-arch);
#endif

