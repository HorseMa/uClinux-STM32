/*
 * Dynamic DMA mapping support.
 *
 * On i386 there is no hardware dynamic DMA address translation,
 * so consistent alloc/free are merely page allocation/freeing.
 * The rest of the dynamic DMA mapping interface is implemented
 * in asm/pci.h.
 */

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/pci.h>
#include <linux/list.h>
#include <linux/highmem.h>
#include <asm/io.h>

#ifndef CONFIG_UCLINUX

void *pci_alloc_consistent(struct pci_dev *hwdev, size_t size, dma_addr_t *dma_handle)
{
	void *ret;
	int gfp = GFP_ATOMIC;

	if (hwdev == NULL || hwdev->dma_mask < 0xffffffff)
		gfp |= GFP_DMA;

	ret = consistent_alloc(gfp, size, dma_handle);

	if (ret != NULL)
		memset(ret, 0, size);

	return ret;
}

void pci_free_consistent(struct pci_dev *hwdev, size_t size, void *vaddr, dma_addr_t dma_handle)
{
	consistent_free(vaddr);
}

dma_addr_t pci_map_single(struct pci_dev *hwdev, void *ptr, size_t size, int direction)
{
	if (direction == PCI_DMA_NONE)
                BUG();

	frv_cache_wback_inv((unsigned long) ptr, (unsigned long) ptr + size);

	return virt_to_bus(ptr);
}

int pci_map_sg(struct pci_dev *hwdev, struct scatterlist *sg, int nents, int direction)
{
	unsigned long dampr2;
	void *vaddr;
	int i;

	if (direction == PCI_DMA_NONE)
                BUG();

	dampr2 = __get_DAMPR(2);

	for (i = 0; i < nents; i++) {
		vaddr = kmap_atomic(sg[i].page, __KM_CACHE);

		frv_dcache_writeback((unsigned long) vaddr,
				     (unsigned long) vaddr + PAGE_SIZE);

	}

	kunmap_atomic(vaddr, __KM_CACHE);
	if (dampr2) {
		__set_DAMPR(2, dampr2);
		__set_IAMPR(2, dampr2);
	}

	return nents;
}


#else /* !CONFIG_UCLINUX */

#if 1
#define PCI_SRAM_START	dma_consistent_mem_start
#define PCI_SRAM_END	dma_consistent_mem_end
#else // Use video RAM on Matrox
#define PCI_SRAM_START	0xe8900000
#define PCI_SRAM_END	0xe8a00000
#endif

struct pci_alloc_record {
	struct list_head list;
	unsigned long ofs;
	unsigned long len;
};

static spinlock_t pci_alloc_lock = SPIN_LOCK_UNLOCKED;
static LIST_HEAD(pci_alloc_list);

void *pci_alloc_consistent(struct pci_dev *hwdev, size_t size,
			   dma_addr_t *dma_handle)
{
	struct list_head *this = &pci_alloc_list;
	struct pci_alloc_record *new;
	unsigned long flags;
	unsigned long start = PCI_SRAM_START;
	unsigned long end;

	if (!PCI_SRAM_START) {
		printk("%s called without any DMA area reserved!\n", __func__);
		return NULL;
	}
	new = kmalloc(sizeof (*new), GFP_ATOMIC);
	if (!new)
		return NULL;

	/* Round up to a reasonable alignment */
	new->len = (size + 31) & ~31;

	spin_lock_irqsave(&pci_alloc_lock, flags);
	
	list_for_each (this, &pci_alloc_list) {
		struct pci_alloc_record *this_r = list_entry(this, struct pci_alloc_record, list);
		end = this_r->ofs;

		if (end - start >= size)
			goto gotone;

		start = this_r->ofs + this_r->len;
	}
	/* Reached end of list. */
	end = PCI_SRAM_END;
	this = &pci_alloc_list;

	if (end - start >= size) {
	gotone:
		new->ofs = start;
		list_add_tail(&new->list, this);
		spin_unlock_irqrestore(&pci_alloc_lock, flags);

		*dma_handle = start;
		return (void *)start;
	}

	kfree(new);
	spin_unlock_irqrestore(&pci_alloc_lock, flags);
	return NULL;
}

void pci_free_consistent(struct pci_dev *hwdev, size_t size,
			 void *vaddr, dma_addr_t dma_handle)
{
	struct pci_alloc_record *rec;
	unsigned long flags;

	spin_lock_irqsave(&pci_alloc_lock, flags);

	list_for_each_entry(rec, &pci_alloc_list, list) {
		if (rec->ofs == dma_handle) {
			list_del(&rec->list);
			kfree(rec);
			spin_unlock_irqrestore(&pci_alloc_lock, flags);
			return;
		}
	}
	spin_unlock_irqrestore(&pci_alloc_lock, flags);
	BUG();
}

dma_addr_t pci_map_single(struct pci_dev *hwdev, void *ptr, size_t size, int direction)
{
	if (direction == PCI_DMA_NONE)
                BUG();

	frv_cache_wback_inv((unsigned long) ptr, (unsigned long) ptr + size);

	return virt_to_bus(ptr);
}

int pci_map_sg(struct pci_dev *hwdev, struct scatterlist *sg, int nents, int direction)
{
	int i;

	for (i = 0; i < nents; i++)
		frv_cache_wback_inv(sg_dma_address(&sg[i]),
				    sg_dma_address(&sg[i]) + sg_dma_len(&sg[i]));

	if (direction == PCI_DMA_NONE)
                BUG();

	return nents;
}

#endif /* !CONFIG_UCLINUX */
