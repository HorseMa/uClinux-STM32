/*
 * linux/include/asm-arm/arch-ixp425/timex.h
 *
 * XScale architecture timex specifications
 */

/*
 * We use IXP425 General purpose timer for our timer needs, it runs at 66,666,666 Hz.
 * The CyberGuard/iVPN hardware has either a 20MHz or 30MHz base clock, to reduce power consumption.
 */
#if defined(CONFIG_IVPN_20MHZ)
#define CLOCK_TICK_RATE (40000000)
#elif defined(CONFIG_MACH_IVPN)
#define CLOCK_TICK_RATE (60000000)
#else
#define CLOCK_TICK_RATE (66000000)
#endif

