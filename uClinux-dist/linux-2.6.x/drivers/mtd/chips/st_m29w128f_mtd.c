/* st_m29w128f_mtd.c */

/*
 * Simple M29W128F NOR flash driver for Linux
 *
 * based on the Embedded Linux book MTD example.
 *
 * Licensed under the GPL.
 *
 * 
 */


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/mtd/map.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/cfi.h>
#include <linux/delay.h>

#include <asm/arch/stm32f10x_conf.h>


/*
We now go into the details of a NOR flash driver for Linux. The file mtd.c
contains the code for a simple NOR flash based on the following assumptions.
 The flash device has a single erase region so that all sectors have the same
size. (An erase region is defined as an area of a chip that contains the
sectors of the same size.)
 The flash chip is accessed using a 4-byte bus width.
 There are no locking, unlocking, suspend, and resume functionalities.
For simplicitys sake we assume that the following information is available
to us as macros or as functions.
 M29W128F_NOR_FLASH_ERASE_SIZE: The flash erase sector size
 M29W128F_NOR_FLASH_SIZE: The flash size
 PROBE_FLASH(): The function that probes if the NOR flash is present at
the specified address
 WRITE_FLASH_ONE_WORD: The function/macro to write a word at a
specified address
 ERASE_FLASH_SECTOR: The function to erase a given sector
 M29W128F_NOR_FLASH_ERASE_TIME: Per-sector erase time in jiffies
First let us put all the header files we want for our flash driver.
*/

#define SIZE_16MiB						23
#define DEVICE_SIZE						(1<<SIZE_16MiB)
#define MANUFACTURER_ST						0x0020
#define M29W128F        					0x2212

#define M29W128F_NOR_FLASH_ERASE_SIZE				(0x10000)

#define WRITE_FLASH_ONE_WORD(map, chip_start, addr, data)	FSMC_NOR_WriteBuffer(data,addr,2)

#define ERASE_FLASH_SECTOR(map, chip_start, addr)		FSMC_NOR_EraseBlock(addr)

#define M29W128F_NOR_FLASH_ERASE_TIME				((uint32_t)1000)

#define M29W128F_NOR_FLASH_SIZE					(16*1024*1024)



struct m29w128f_private_info_struct
{
	int number_of_chips; /* Number of flash chips */
	int chipshift; /* Size of each flash */
	struct flchip *chips;
} ;

static struct mtd_info * m29w128f_probe(struct map_info *);
static void m29w128f_destroy(struct mtd_info *);
static int m29w128f_flash_read(struct mtd_info *, loff_t , size_t ,size_t *, u_char *);
static int m29w128f_flash_erase(struct mtd_info *,struct erase_info *);
static int m29w128f_flash_write(struct mtd_info *, loff_t ,size_t , size_t *, const u_char *);
static void m29w128f_flash_sync(struct mtd_info *);


static struct mtd_chip_driver m29w128f_chipdrv =
{
	.probe = m29w128f_probe,
	.destroy = m29w128f_destroy,
	.name = "m29w128f_probe",
	.module = THIS_MODULE
};

static int probe_flash(struct map_info *map)
{
	/*NOR_IDTypeDef NOR_ID;
	FSMC_NOR_ReadID(&NOR_ID);
	if((NOR_ID.Manufacturer_Code==MANUFACTURER_ST)&&(NOR_ID.Device_Code2==M29W128F))
	{
		FSMC_NOR_ReturnToReadMode();
		return 1;
	}
	else 
		return 0;*/
		
	//xip mode is activated, we can't probe the flash
	//so this why we only return the number of chip on the STM3210E-EVAL board.
	return 1;
}

static void Write_One_Word(struct map_info *map, u32 chip_start, u32 addr, u16* data)
{
	/*no write possible du to xip mode.*/
	printk("no write possible due to xip mode.\n");
	
	//while(FSMC_NOR_GetStatus(500)!=NOR_SUCCESS);	
	//WRITE_FLASH_ONE_WORD(map_info,chip_start,addr,data);
}

static struct mtd_info *m29w128f_probe(struct map_info *map)
{
	struct mtd_info * mtd = kmalloc(sizeof(*mtd), GFP_KERNEL);
	unsigned int i;
	unsigned long size;
	struct m29w128f_private_info_struct * m29w128f_private_info = 
			kmalloc(sizeof(struct m29w128f_private_info_struct), GFP_KERNEL);
	
	if(!m29w128f_private_info)
		return NULL;
		
	memset(m29w128f_private_info, 0, sizeof(*m29w128f_private_info));
	
	/* The probe function returns the number of chips identified */
	
	m29w128f_private_info->number_of_chips = probe_flash(map);
	
	if(!m29w128f_private_info->number_of_chips)
	{
		kfree(mtd);
		return NULL;
	}
	
	/* Initialize mtd structure */
	memset(mtd, 0, sizeof(*mtd));
	
	mtd->erasesize = M29W128F_NOR_FLASH_ERASE_SIZE;
	mtd->size = m29w128f_private_info->number_of_chips * M29W128F_NOR_FLASH_SIZE;
	for(size = mtd->size; size > 1; size >>= 1)
		m29w128f_private_info->chipshift++;
	mtd->priv = map;
	mtd->type = MTD_NORFLASH;
	mtd->flags = MTD_CAP_NORFLASH;
	mtd->name = "M29W128F NOR FLASH";
	mtd->erase = m29w128f_flash_erase;
	mtd->read = m29w128f_flash_read;
	mtd->write = m29w128f_flash_write;
	mtd->writesize=1;
	mtd->sync = m29w128f_flash_sync;
	
	m29w128f_private_info->chips = kmalloc(sizeof(struct flchip) * m29w128f_private_info->number_of_chips, GFP_KERNEL);
	memset(m29w128f_private_info->chips, 0, sizeof(*(m29w128f_private_info->chips)));
	
	for(i=0; i < m29w128f_private_info->number_of_chips; i++)
	{
		m29w128f_private_info->chips[i].start = (M29W128F_NOR_FLASH_SIZE * i);
		m29w128f_private_info->chips[i].state = FL_READY;
		m29w128f_private_info->chips[i].mutex = &m29w128f_private_info->chips[i]._spinlock;
		
		init_waitqueue_head(&m29w128f_private_info->chips[i].wq);
		spin_lock_init(&m29w128f_private_info->chips[i]._spinlock);
		
		m29w128f_private_info->chips[i].erase_time = M29W128F_NOR_FLASH_ERASE_TIME;
	}
	
	map->fldrv = &m29w128f_chipdrv;
	map->fldrv_priv = m29w128f_private_info;
	
	printk("Probed and found the STM3210E-EVAL NOR flash chip\n");
	return mtd;
}

static inline int m29w128f_flash_read_one_chip(struct map_info *map,
						struct flchip *chip, loff_t addr, size_t len, u_char *buf)
{
	DECLARE_WAITQUEUE(wait, current);
	again:
	spin_lock(chip->mutex);
	if(chip->state != FL_READY)
	{
		set_current_state(TASK_UNINTERRUPTIBLE);
		add_wait_queue(&chip->wq, &wait);
		spin_unlock(chip->mutex);
		schedule();
		remove_wait_queue(&chip->wq, &wait);
		if(signal_pending(current))
			return -EINTR;
		goto again;
	}
	addr += chip->start;
	chip->state = FL_READY;
	map_copy_from(map, buf, addr, len);
	wake_up(&chip->wq);
	spin_unlock(chip->mutex);
	return 0;
}
static int m29w128f_flash_read(struct mtd_info *mtd, loff_t from,
				size_t len,size_t *retlen, u_char *buf)
{
	struct map_info *map = mtd->priv;
	struct m29w128f_private_info_struct *priv = map->fldrv_priv;
	int chipnum = 0;
	int ret = 0;
	unsigned int ofs;
	*retlen = 0;
	/* Find the chip number and offset for the first chip */
	chipnum = (from >> priv->chipshift);
	ofs = from & ((1 << priv->chipshift) - 1);
	while(len)
	{
		unsigned long to_read;
		if(chipnum >= priv->number_of_chips)
			break;
		/* Check whether the read spills over to the next chip */
		if( (len + ofs - 1) >> priv->chipshift)
			to_read = (1 << priv->chipshift) - ofs;
		else
			to_read = len;
		
		if( (ret = m29w128f_flash_read_one_chip(map, &priv->chips[chipnum],ofs, to_read, buf)))
			break;
		*retlen += to_read;
		len -= to_read;
		buf += to_read;
		ofs=0;
		chipnum++;
	}
	return ret;
}

static inline int m29w128f_flash_write_oneword(struct map_info *map,
						struct flchip *chip, loff_t addr, __u32 datum)
{
	DECLARE_WAITQUEUE(wait, current);
again:
	spin_lock(chip->mutex);
	if(chip->state != FL_READY)
	{
		set_current_state(TASK_UNINTERRUPTIBLE);
		add_wait_queue(&chip->wq, &wait);
		spin_unlock(chip->mutex);
		schedule();
		remove_wait_queue(&chip->wq, &wait);
		if(signal_pending(current))
			return -EINTR;
		goto again;
	}
	addr += chip->start;
	chip->state = FL_WRITING;
	Write_One_Word(map, chip->start, addr,(u16*) &datum);
	chip->state = FL_READY;
	wake_up(&chip->wq);
	spin_unlock(chip->mutex);
return 0;
}

static int m29w128f_flash_write(struct mtd_info *mtd, loff_t from,
				size_t len,size_t *retlen, const u_char *buf)
{
	struct map_info *map = mtd->priv;
	struct m29w128f_private_info_struct *priv = map->fldrv_priv;
	int chipnum = 0;
	union {
		unsigned int idata;
		char cdata[4]; }wbuf;
	unsigned int ofs;
	int ret;
	*retlen = 0;
	chipnum = (from >> priv->chipshift);
	ofs = from & ((1 << priv->chipshift) - 1);
	
	/* First check if the first word to be written is aligned */
	if(ofs & 3)
	{
		unsigned int from_offset = ofs & (~3);
		unsigned int orig_copy_num = ofs - from_offset;
		unsigned int to_copy_num = (4 - orig_copy_num);
		unsigned int i, len=0;
		map_copy_from(map, wbuf.cdata, from_offset +priv->chips[chipnum].start, 4);
		/* Overwrite with the new contents from buf[] */
		for(i=0; i < to_copy_num; i++)
			wbuf.cdata[orig_copy_num + i] = buf[i];
			
		if((ret = m29w128f_flash_write_oneword(map, &priv->chips[chipnum],from_offset, wbuf.idata)) < 0)
			return ret;
		ofs += i;
		buf += i;
		*retlen += i;
		len -= i;
		if(ofs >> priv->chipshift)
		{
			chipnum++;
			ofs = 0;
		}
	}
	/* Now write all the aligned words */
	while(len / 4)
	{
		memcpy(wbuf.cdata, buf, 4);
		if((ret = m29w128f_flash_write_oneword(map, &priv->chips[chipnum],ofs, wbuf.idata)) < 0)
			return ret;
		ofs += 4;
		buf += 4;
		*retlen += 4;
		len -= 4;
		if(ofs >> priv->chipshift)
		{
			chipnum++;
			ofs = 0;
		}
	}
	/* Write the last word */
	if(len)
	{
		unsigned int i=0;
		map_copy_from(map, wbuf.cdata, ofs + priv->chips[chipnum].start,4);
		for(; i<len; i++)
			wbuf.cdata[i] = buf[i];
		if((ret = m29w128f_flash_write_oneword(map, &priv->chips[chipnum],ofs, wbuf.idata)) < 0)
			return ret;
		*retlen += i;
	}
return 0;
}

static int m29w128f_flash_erase_one_block(struct map_info *map,
					struct flchip *chip, unsigned long addr)
{
	DECLARE_WAITQUEUE(wait, current);
again:
	spin_lock(chip->mutex);
	if(chip->state != FL_READY)
	{
		set_current_state(TASK_UNINTERRUPTIBLE);
		add_wait_queue(&chip->wq, &wait);
		spin_unlock(chip->mutex);
		schedule();
		remove_wait_queue(&chip->wq, &wait);
		if(signal_pending(current))
			return -EINTR;
	goto again;
	}
	chip->state = FL_ERASING;
	addr += chip->start;
	ERASE_FLASH_SECTOR(map, chip->start, addr);
	spin_unlock(chip->mutex);
	schedule_timeout(chip->erase_time);
	if(signal_pending(current))
		return -EINTR;
	/* We have been woken after the timeout. Take the mutex to proceed */
	spin_lock(chip->mutex);
	/* Add any error checks if the flash sector has not been erased. */
	/* We assume that here the flash erase has been completed */
	chip->state = FL_READY;
	wake_up(&chip->wq);
	spin_unlock(chip->mutex);
return 0;
}
static int m29w128f_flash_erase(struct mtd_info *mtd,
				struct erase_info *instr)
{
	struct map_info *map = mtd->priv;
	struct m29w128f_private_info_struct *priv = map->fldrv_priv;
	int chipnum = 0;
	unsigned long addr;
	int len;
	int ret;
	
	/* Some error checkings initially */
	if( (instr->addr > mtd->size) ||
		((instr->addr + instr->len) > mtd->size) ||
			(instr->addr & (mtd->erasesize -1)))
				return -EINVAL;
	/* Find the chip number for the first chip */
	chipnum = (instr->addr >> priv->chipshift);
	addr = instr->addr & ((1 << priv->chipshift) - 1);
	len = instr->len;
	while(len)
	{
		if( (ret = m29w128f_flash_erase_one_block(map, &priv->chips[chipnum], addr)) < 0)
			return ret;
		addr += mtd->erasesize;
		len -= mtd->erasesize;
		if(addr >> priv->chipshift)
		{
			addr = 0;
			chipnum++;
		}
	}
	instr->state = MTD_ERASE_DONE;
	if(instr->callback)
		instr->callback(instr);
return 0;
}

static void m29w128f_flash_sync(struct mtd_info *mtd)
{
	struct map_info *map = mtd->priv;
	struct m29w128f_private_info_struct *priv = map->fldrv_priv;
	struct flchip *chip;
	int i;
	
	DECLARE_WAITQUEUE(wait, current);
	for(i=0; i< priv->number_of_chips;i++)
	{
		chip = &priv->chips[i];
again:
		spin_lock(chip->mutex);
		switch(chip->state)
		{
			case FL_READY:
			case FL_STATUS:
				chip->oldstate = chip->state;
				chip->state = FL_SYNCING;
				break;
			case FL_SYNCING:
				spin_unlock(chip->mutex);
					break;
			default:
				add_wait_queue(&chip->wq, &wait);
				spin_unlock(chip->mutex);
				schedule();
				remove_wait_queue(&chip->wq, &wait);
				goto again;
		}
	}
	for(i--; i >=0; i--)
	{
		chip = &priv->chips[i];
		spin_lock(chip->mutex);
		if(chip->state == FL_SYNCING)
		{
			chip->state = chip->oldstate;
			wake_up(&chip->wq);
		}
		spin_unlock(chip->mutex);
	}
}

static void m29w128f_destroy(struct mtd_info *mtd)
{
	struct m29w128f_private_info_struct *priv =((struct map_info *)mtd->priv)->fldrv_priv;
	kfree(priv->chips);
}
/*The following are the initialization and exit functions.*/
int __init m29w128f_flash_init(void)
{
	register_mtd_chip_driver(&m29w128f_chipdrv);
	return 0;
}
void __exit m29w128f_flash_exit(void)
{
	unregister_mtd_chip_driver(&m29w128f_chipdrv);
}

module_init(m29w128f_flash_init);
module_exit(m29w128f_flash_exit);


MODULE_AUTHOR("MCD Application Team");
MODULE_DESCRIPTION("STM32 M29W128F driver");
MODULE_LICENSE("GPL");