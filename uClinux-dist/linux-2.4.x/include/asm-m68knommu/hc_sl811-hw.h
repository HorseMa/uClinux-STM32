/*
File: include/asm-arm/hc_sl811-hw.h
*/

#include <asm/coldfire.h>
#include <asm/mcfsim.h>


/* The base_addr, data_reg_addr, and irq number are board specific.
 * NOTE: values need to modify for different development boards
 */

#if defined(CONFIG_SIGNAL_MCP751)
#define SIZEOF_IO_REGION	256	/* Size for request/release region */
static int base_addr	 = 0x46000000;
static int data_reg_addr = 0x46000001;
static int irq		 = 90;
#define MAX_CONTROLERS  1

MODULE_PARM (base_addr, "i");
MODULE_PARM_DESC (base_addr, "sl811 base address 0x46000000");
MODULE_PARM (data_reg_addr, "i");
MODULE_PARM_DESC (data_reg_addr, "sl811 data register address 0x46000001");
MODULE_PARM (irq, "i");
MODULE_PARM_DESC (irq, "IRQ 90 (default)");
#endif

/*
 * Low level: Read from Data port [arm]
 */
static __u8 inline sl811_read_data (hcipriv_t *hp)
{
	__u8 data;
	data = readb (hp->hcport2);
	rmb ();
	return (data);
}

/*
 * Low level: Write to index register [arm]
 */
static void inline sl811_write_index (hcipriv_t *hp, __u8 index)
{
	writeb (index, hp->hcport);
	wmb ();
}

/*
 * Low level: Write to Data port [arm]
 */
static void inline sl811_write_data (hcipriv_t *hp, __u8 data)
{
	writeb (data, hp->hcport2);
}

/*
 * Low level: Write to index register and data port [arm]
 */
static void inline sl811_write_index_data (hcipriv_t *hp, __u8 index, __u8 data)
{
	writeb (index, hp->hcport);
	writeb (data, hp->hcport2);
	wmb ();
}

/*****************************************************************
 *
 * Function Name: init_irq [arm]
 *
 * This function is board specific.  It sets up the interrupt to
 * be an edge trigger and trigger on the rising edge
 *
 * Input: none
 *
 * Return value  : none
 *
 *****************************************************************/
static void inline init_irq (void)
{
#if defined(CONFIG_SIGNAL_MCP751)
	// ICR4 enable INT5
	{
	volatile unsigned long *icrp;

	// PORTD configure pin K3 for INT5
	*((volatile unsigned long  *) (MCF_MBAR+MCFSIM_PDCNT)) |= 0x000000C0;

	// transition low to high
	*((volatile unsigned long  *) (MCF_MBAR+MCFSIM_PITR)) |= 0x00000040;

	icrp = (volatile unsigned long *) (MCF_MBAR + MCFSIM_ICR4);
	*icrp = (*icrp & 0x70777777) | 0x0d000000; // INT5
	}
#endif
}

/*****************************************************************
 *
 * Function Name: release_regions [arm]
 *
 * This function is board specific.  It frees all io address.
 *
 * Input: hcipriv_t *
 *
 * Return value  : none
 *
 *****************************************************************/
static void inline sl811_release_regions (hcipriv_t *hp)
{
}

/*****************************************************************
 *
 * Function Name: request_regions [arm]
 *
 * This function is board specific. It request all io address and
 * maps into memory (if can).
 *
 * Input: hcipriv_t *
 *
 * Return value  : 0 = OK
 *
 *****************************************************************/
static int inline sl811_request_regions (hcipriv_t *hp, int addr1, int addr2)
{
	hp->hcport = addr1;
	hp->hcport2 = addr2;

	return 0;
}

