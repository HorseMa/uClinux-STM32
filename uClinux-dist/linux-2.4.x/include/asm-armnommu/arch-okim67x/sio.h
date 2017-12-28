/*
 * linux/include/asm-arm/arch-oki/sio.h
 * for OKI m67x series SIO port (NOT UART)
 * 200 V. R. Sanders
 */

#define OKI_SIOBUF	(0xB8002000)
#define OKI_SIOSTA	(0xB8002004)
#define OKI_SIOCON	(0xB8002008)
#define OKI_SIOBCN	(0xB800200C)
#define OKI_SIOBT	(0xB8002014)
#define OKI_SIOTCN	(0xB8002018)

#define OKI_SIOSTA_FERR		(1<<0)
#define OKI_SIOSTA_OERR		(1<<1)
#define OKI_SIOSTA_PERR		(1<<2)
#define OKI_SIOSTA_RVIRQ	(1<<4)
#define OKI_SIOSTA_TRIRQ	(1<<5)

#define OKI_SIOSTA_ANYIRQ (OKI_SIOSTA_TRIRQ|OKI_SIOSTA_RVIRQ)

#define OKI_SIOCON_LN7		(1<<0)
#define OKI_SIOCON_PEN		(1<<1)
#define OKI_SIOCON_EVN		(1<<2)
#define OKI_SIOCON_TSTB		(1<<3)

#define OKI_SIOBCN_BGRUN	(1<<4)

#define OKI_SIOTCN_MFERR	(1<<0)
#define OKI_SIOTCN_MPERR	(1<<1)
#define OKI_SIOTCN_LBTST	(1<<7)
