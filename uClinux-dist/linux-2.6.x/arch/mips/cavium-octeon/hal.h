/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2004-2007 Cavium Networks
 */
#ifndef __CAVIUM_OCTEON_HAL_H
#define __CAVIUM_OCTEON_HAL_H

#include "octeon-hal-read-write.h"

#ifndef __BYTE_ORDER
#  error "__BYTE_ORDER not set"
#endif
#ifndef __BIG_ENDIAN
#  error "__BIG_ENDIAN not set"
#endif

extern uint64_t octeon_bootmem_alloc_range_phys(uint64_t size,
						uint64_t alignment,
						uint64_t min_addr,
						uint64_t max_addr,
						int do_locking);
extern void *octeon_bootmem_alloc(uint64_t size, uint64_t alignment,
				  int do_locking);
extern void *octeon_bootmem_alloc_range(uint64_t size, uint64_t alignment,
					uint64_t min_addr, uint64_t max_addr,
					int do_locking);
extern void *octeon_bootmem_alloc_named(uint64_t size, uint64_t alignment,
					char *name);
extern void *octeon_bootmem_alloc_named_range(uint64_t size, uint64_t min_addr,
					      uint64_t max_addr, uint64_t align,
					      char *name);
extern void *octeon_bootmem_alloc_named_address(uint64_t size, uint64_t address,
						char *name);
extern int octeon_bootmem_free_named(char *name);
extern void octeon_bootmem_lock(void);
extern void octeon_bootmem_unlock(void);

extern int octeon_is_simulation(void);
extern int octeon_is_pci_host(void);
extern int octeon_usb_is_ref_clk(void);
extern uint64_t octeon_get_clock_rate(void);
extern const char *octeon_board_type_string(void);
extern const char *octeon_get_pci_interrupts(void);
extern int octeon_get_southbridge_interrupt(void);
extern int octeon_get_boot_coremask(void);
extern uint64_t octeon_get_dram_size(void);
extern int octeon_get_boot_num_arguments(void);
extern const char *octeon_get_boot_argument(int arg);
extern void octeon_hal_setup_reserved32(void);
struct octeon_cop2_state;
extern unsigned long octeon_crypto_enable(struct octeon_cop2_state *state);
extern void octeon_crypto_disable(struct octeon_cop2_state *state,
				  unsigned long flags);

typedef union {
	uint64_t u64;
	struct {
		/* RO 1 = BIST fail, 0 = BIST pass */
		uint64_t tlbbist:1;
		/* RO 1 = BIST fail, 0 = BIST pass */
		uint64_t l1cbist:1;
		/* RO 1 = BIST fail, 0 = BIST pass */
		uint64_t l1dbist:1;
		/* RO 1 = BIST fail, 0 = BIST pass */
		uint64_t dcmbist:1;
		/* RO 1 = BIST fail, 0 = BIST pass */
		uint64_t ptgbist:1;
		/* RO 1 = BIST fail, 0 = BIST pass */
		uint64_t wbfbist:1;
		/* Reserved */
		uint64_t reserved:22;
		/* R/W If set, marked write-buffer entries time out
		 * the same as as other entries; if clear, marked
		 * write-buffer entries use the maximum timeout. */
		uint64_t dismarkwblongto:1;
		/* R/W If set, a merged store does not clear the
		 * write-buffer entry timeout state. */
		uint64_t dismrgclrwbto:1;
		/* R/W Two bits that are the MSBs of the resultant
		 * CVMSEG LM word location for an IOBDMA. The other 8
		 * bits come from the SCRADDR field of the IOBDMA. */
		uint64_t iobdmascrmsb:2;
		/* R/W If set, SYNCWS and SYNCS only order marked
		 * stores; if clear, SYNCWS and SYNCS only order
		 * unmarked stores. SYNCWSMARKED has no effect when
		 * DISSYNCWS is set. */
		uint64_t syncwsmarked:1;
		/* R/W If set, SYNCWS acts as SYNCW and SYNCS acts as
		 * SYNC. */
		uint64_t dissyncws:1;
		/* R/W If set, no stall happens on write buffer
		 * full. */
		uint64_t diswbfst:1;
		/* R/W If set (and SX set), supervisor-level
		 * loads/stores can use XKPHYS addresses with
		 * VA<48>==0 */
		uint64_t xkmemenas:1;
		/* R/W If set (and UX set), user-level loads/stores
		 * can use XKPHYS addresses with VA<48>==0 */
		uint64_t xkmemenau:1;
		/* R/W If set (and SX set), supervisor-level
		 * loads/stores can use XKPHYS addresses with
		 * VA<48>==1 */
		uint64_t xkioenas:1;
		/* R/W If set (and UX set), user-level loads/stores
		 * can use XKPHYS addresses with VA<48>==1 */
		uint64_t xkioenau:1;
		/* R/W If set, all stores act as SYNCW (NOMERGE must
		 * be set when this is set) RW, reset to 0. */
		uint64_t allsyncw:1;
		/* R/W If set, no stores merge, and all stores reach
		 * the coherent bus in order. */
		uint64_t nomerge:1;
		/* R/W Selects the bit in the counter used for DID
		 * time-outs 0 = 231, 1 = 230, 2 = 229, 3 =
		 * 214. Actual time-out is between 1� and 2� this
		 * interval. For example, with DIDTTO=3, expiration
		 * interval is between 16K and 32K. */
		uint64_t didtto:2;
		/* R/W If set, the (mem) CSR clock never turns off. */
		uint64_t csrckalwys:1;
		/* R/W If set, mclk never turns off. */
		uint64_t mclkalwys:1;
		/* R/W Selects the bit in the counter used for write
		 * buffer flush time-outs (WBFLT+11) is the bit
		 * position in an internal counter used to determine
		 * expiration. The write buffer expires between 1� and
		 * 2� this interval. For example, with WBFLT = 0, a
		 * write buffer expires between 2K and 4K cycles after
		 * the write buffer entry is allocated. */
		uint64_t wbfltime:3;
		/* R/W If set, do not put Istream in the L2 cache. */
		uint64_t istrnol2:1;
		/* R/W The write buffer threshold. */
		uint64_t wbthresh:4;
		/* Reserved */
		uint64_t reserved2:2;
		/* R/W If set, CVMSEG is available for loads/stores in
		 * kernel/debug mode. */
		uint64_t cvmsegenak:1;
		/* R/W If set, CVMSEG is available for loads/stores in
		 * supervisor mode. */
		uint64_t cvmsegenas:1;
		/* R/W If set, CVMSEG is available for loads/stores in
		 * user mode. */
		uint64_t cvmsegenau:1;
		/* R/W Size of local memory in cache blocks, 54 (6912
		 * bytes) is max legal value. */
		uint64_t lmemsz:6;
	} s;
} octeon_cvmemctl_t;

static inline void octeon_led_write(int bank, uint32_t data)
{
	cvmx_write_csr(CVMX_LED_UDD_DATX(bank), data);
}

static inline uint32_t octeon_led_read(int bank)
{
	return cvmx_read_csr(CVMX_LED_UDD_DATX(bank));
}

static inline void octeon_led_set(int bank, int bit)
{
	cvmx_write_csr(CVMX_LED_UDD_DAT_SETX(bank), 1 << bit);
}

static inline void octeon_led_clear(int bank, int bit)
{
	cvmx_write_csr(CVMX_LED_UDD_DAT_CLRX(bank), 1 << bit);
}

static inline unsigned long octeon_get_cycles(void)
{
	unsigned long result;
	asm volatile ("rdhwr %0,$31\n\t"
#ifndef CONFIG_64BIT
		"sll %0,0"
#endif
		: "=r" (result));
	return result;
}

extern void octeon_write_lcd(const char *s);
extern void octeon_check_cpu_bist(void);
extern void octeon_hal_init(void);
extern int octeon_get_boot_uart(void);
extern int octeon_get_boot_debug_flag(void);
extern void octeon_poweroff(void);

struct uart_port;
extern unsigned int octeon_serial_in(struct uart_port *, int);
extern void octeon_serial_out(struct uart_port *, int, int);

#endif
