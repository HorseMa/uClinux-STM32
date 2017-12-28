/*
 * mtdcnxt - a hack for read/write-access to a small chunk of
 * flash space via the conexant flash routines CnxtFlashOpen(),
 * CnxtFlashReadRequest() and CnxtFlashWriteRequest()
 *
 * Please note that whenever you read or write to the device 
 * (/dev/mtdc0), the flash is not directly accessed, but a copy
 * of the data in RAM is modified
 * To actually write the content to flash you have to write the string
 * "SaveToFlash" to /dev/mtdc0 (e.g. by executing
 * "echo SaveToFlash >/dev/mtdc0" at the shell prompt)
 * Likewise to read the flash and copy it into the RAM buffer,
 * write "ReadFrFlash" to /dev/mtdc0; also note that this is NOT
 * automatically done at device startup, so before you make changes
 * to the data, issue the read command
 * As this method of flash rw-access mixes up data and commands,
 * i regard it as an HACK
 *
 * In addition to the flash access, also the modem LED and the modem
 * reset is controlled via this device:
 * echo "ResetModem***" >/dev/mtdc0 -- will reset the modem
 * echo "LedOn***" >/dev/mtdc0      -- will turn the modem LED on
 * echo "LedOff***" >/dev/mtdc0     -- will turn the modem LED off
 *
 * This code is heavily based on mtdram.c and has been extended
 * by Actiontec (http://www.actiontec.com/) for their Dual PC Modem
 *
 * It's noteworthy that Actiontec never officially released this
 * code (it's NOT part of their DPCM GPL release); the file has
 * been posted by Bruce D. Lightner <lightner@lightner.net> to
 * the ActionHack Mailing List (ActionHack@express.org, archive
 * at http://www.express.org/mailman/listinfo/actionhack) as part
 * of his DualPcModemGplPatch; he declared the source of the file
 * as unspecified and he also noted that the "This code is GPL"
 * notice in the file has been deleted (probably be Actiontec); but
 * as the code is clearly derived from GPL code it's of course
 * still GPL
 *
 * Author: Alexander Larsson <alex@cendio.se>
 * Conexant flash hack by: Actiontec (http://www.actiontec.com/)
 * Code origin notice and cleanups by: Johann Hanne <jhml@gmx.net>
 *
 * Copyright (c) 1999 Alexander Larsson <alex@cendio.se>
 * Copyright (c) 2003 Actiontec (http://www.actiontec.com/)
 * Copyright (c) 2005 Johann Hanne <jhml@gmx.net>
 *
 * This code is GPL
 *
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/ioport.h>
#include <linux/mtd/compatmac.h>
#include <linux/mtd/mtd.h>
#include <asm/arch/bsptypes.h>
#include <asm/arch/cnxtflash.h>
#include <asm/arch/gpio.h>

#define SUPPORT_SST_FLASH

#include <asm/io.h>

#define MTDCNXT_TOTAL_SIZE 2048
#define MTDCNXT_ERASE_SIZE 1024

/* We could store these in the mtd structure, but we only support 1 device */
static struct mtd_info *mtd_info;

static int ram_erase(struct mtd_info *mtd, struct erase_info *instr)
{
  DEBUG(MTD_DEBUG_LEVEL2, "ram_erase(pos:%ld, len:%ld)\n", (long)instr->addr, (long)instr->len);
  if (instr->addr + instr->len > mtd->size) {
    DEBUG(MTD_DEBUG_LEVEL1, "ram_erase() out of bounds (%ld > %ld)\n", (long)(instr->addr + instr->len), (long)mtd->size);
    return -EINVAL;
  }
	
  memset((char *)mtd->priv + instr->addr, 0xff, instr->len);
	
  instr->state = MTD_ERASE_DONE;

  if (instr->callback)
    (*(instr->callback))(instr);
  return 0;
}

static int ram_point(struct mtd_info *mtd, loff_t from, size_t len,
                     size_t *retlen, u_char **mtdbuf)
{
  if (from + len > mtd->size)
    return -EINVAL;
	
  *mtdbuf = mtd->priv + from;
  *retlen = len;
  return 0;
}

static void ram_unpoint(struct mtd_info *mtd, u_char *addr, loff_t from,
                        size_t len)
{
  DEBUG(MTD_DEBUG_LEVEL2, "ram_unpoint\n");
}

static int ram_read(struct mtd_info *mtd, loff_t from, size_t len,
                    size_t *retlen, u_char *buf)
{
  DEBUG(MTD_DEBUG_LEVEL2, "ram_read(pos:%ld, len:%ld)\n", (long)from, (long)len);
  if (from + len > mtd->size) {
    DEBUG(MTD_DEBUG_LEVEL1, "ram_read() out of bounds (%ld > %ld)\n", (long)(from + len), (long)mtd->size);
    return -EINVAL;
  }

  memcpy(buf, mtd->priv + from, len);

  *retlen=len;
  return 0;
}

/*
 * ACTIONTEC IMPORTANT NOTICE:
 *
 * Don't expect you can read full 2 * 1024 bytes, because you will lose 
 * sizeof(CNXT_FLASH_SEGMENT_T) == about 128 bytes
 *
 * But we don't give any failed to read msg.
 * So the maximum size is about
 *      2 * 1024 - 128 bytes
 */
static char savetoflash[]="SaveToFlash";
static char readfrflash[]="ReadFrFlash";
static char resetmodem[]="ResetModem***";
static char ledon[]="LedOn***";
static char ledoff[]="LedOff***";
static int ram_write(struct mtd_info *mtd, loff_t to, size_t len,
                     size_t *retlen, const u_char *buf)
{
  CNXT_FLASH_STATUS_E flashStatus;
  CNXT_FLASH_SEGMENT_E segment = CNXT_FLASH_SEGMENT_3;

  DEBUG(MTD_DEBUG_LEVEL2, "ram_write(pos:%ld, len:%ld)\n", (long)to, (long)len);
  if (to + len > mtd->size) {
    DEBUG(MTD_DEBUG_LEVEL1, "ram_write() out of bounds (%ld > %ld)\n", (long)(to + len), (long)mtd->size);
    return -EINVAL;
  }

  DEBUG(MTD_DEBUG_LEVEL2, "mtdcnxt write: '%s'\n", buf);

  if (strstr(buf, ledon)) {
    WriteGPIOData(GPIO19, 0);
    DEBUG(MTD_DEBUG_LEVEL2, "Modem LED ON\n");
    memset(buf,'\0',len);
    *retlen=len;
    return 0;
  }

  if (strstr(buf, ledoff)) {
    WriteGPIOData(GPIO19, 1);
    DEBUG(MTD_DEBUG_LEVEL2, "Modem LED OFF\n");
    memset(buf,'\0',len);
    *retlen=len;
    return 0;
  }

#define LED_DELAY_PERIOD 30 /* should be 15/25 mapping reset / ready */

  if (strstr(buf, resetmodem)) {
    /* GPIO17 is used for modem reset for TIBURON */
    WriteGPIOData(GPIO17, 0);
    TaskDelayMsec(LED_DELAY_PERIOD);
    DEBUG(MTD_DEBUG_LEVEL2, "Modem reset\n");
    WriteGPIOData(GPIO17, 1);
    TaskDelayMsec(LED_DELAY_PERIOD);
    memset(buf,'\0',len);
    *retlen=len;
    return 0;
  }

  if (strstr(buf, savetoflash)) {

try_avoid_CRC_error:

    flashStatus = CnxtFlashOpen(segment);

    switch (flashStatus) {
    case CNXT_FLASH_DATA_ERROR:
      printk("Open to Flash Data Error (Open in mtdc for write): "
             "CNXT_FLASH_SEGMENT_%d\n", segment+1);
      break;

    case CNXT_FLASH_DATA_CRC_ERROR:
      printk("Open to Flash Data CRC Error (Open in mtdc for write): "
             "CNXT_FLASH_SEGMENT_%d\n", segment+1);
      flashStatus = CnxtFlashWriteRequest(
                                          segment, 
                                          (UINT16 *)mtd->priv,
                                          (MTDCNXT_TOTAL_SIZE)
                                         );
      goto try_avoid_CRC_error;
      break;

    case CNXT_FLASH_DATA_SIZE_ERROR:
      printk("Open to Flash Data SIZE Error (Open in mtdc for write): "
             "CNXT_FLASH_SEGMENT_%d\n", segment+1);
      break;

    case CNXT_FLASH_OWNER_ERROR:
      printk("Open to Wrong owner for requested Flash Segment!\n");
      break;

    default:
      DEBUG(MTD_DEBUG_LEVEL2, "Open to Flash Data Success: "
            "CNXT_FLASH_SEGMENT_%d\n", segment+1);
    }

    while (CnxtFlashWriteRequest(segment, (UINT16 *)mtd->priv,
                                 MTDCNXT_TOTAL_SIZE)==FALSE) {
      printk("Error writing to flash, trying again\n");
    }
    memset(buf, '\0', len);
    /* We must return here, otherwise we will overwrite the data with
       the write command */
    *retlen=len;
    return 0;
  }

  if (strstr(buf, readfrflash)) {

    flashStatus = CnxtFlashOpen(segment);

    switch (flashStatus) {
    case CNXT_FLASH_DATA_CRC_ERROR:
      printk("Open to Flash Data CRC Error (Open in mtdc for read): "
             "CNXT_FLASH_SEGMENT_%d\n", segment+1);
      break;

    case CNXT_FLASH_DATA_SIZE_ERROR:
      printk("Open to Flash Data SIZE Error (Open in mtdc for read): "
             "CNXT_FLASH_SEGMENT_%d\n", segment+1);
      break;

    case CNXT_FLASH_DATA_ERROR:
      printk("Open to Flash Data Error (Open in mtdc for read): "
             "CNXT_FLASH_SEGMENT_%d\n", segment+1);
      break;

    case CNXT_FLASH_OWNER_ERROR:
      printk("Open to Wrong owner for requested Flash Segment!\n");
      break;

    default:
      DEBUG(MTD_DEBUG_LEVEL2, "Open to Flash Data Success: "
            "CNXT_FLASH_SEGMENT_%d\n", segment+1);
    }

    while (CnxtFlashReadRequest(segment, (UINT16 *)mtd->priv,
                                MTDCNXT_TOTAL_SIZE)==FALSE) {
      printk("Error reading from flash, trying again\n");
    }
    memset(buf, '\0', len);
    /* We must return here, otherwise we will overwrite the data with
       the read command */
    *retlen=len;
    return 0;
  }

  memcpy ((char *)mtd->priv + to, buf, len);

  *retlen=len;
  return 0;
}

static void __exit cleanup_mtdcnxt(void)
{
  if (mtd_info) {
    del_mtd_device(mtd_info);
    if (mtd_info->priv)
      vfree(mtd_info->priv);
    kfree(mtd_info);
  }
}

int mtdcnxt_init_device(struct mtd_info *mtd, void *mapped_address, 
                       unsigned long size, char *name)
{
   memset(mtd, 0, sizeof(*mtd));

   /* Setup the MTD structure */
   mtd->name = name;
   mtd->type = MTD_RAM;
   mtd->flags = MTD_CAP_RAM;
   mtd->size = size;
   mtd->erasesize = MTDCNXT_ERASE_SIZE;
   mtd->priv = mapped_address;

   mtd->module = THIS_MODULE;
   mtd->erase = ram_erase;
   mtd->point = ram_point;
   mtd->unpoint = ram_unpoint;
   mtd->read = ram_read;
   mtd->write = ram_write;

   if (add_mtd_device(mtd)) {
     return -EIO;
   }
   
   return 0;
}

int __init init_mtdcnxt(void)
{
  void *addr;
  int err;

  /* Just for init flash */
  CNXT_FLASH_STATUS_E flashStatus;
  CNXT_FLASH_SEGMENT_E segment = CNXT_FLASH_SEGMENT_3;

#ifdef SUPPORT_SST_FLASH
  setBootsectorBlocksize();
#endif /* SUPPORT_SST_FLASH */
  flashStatus = CnxtFlashOpen(segment);

  /* Allocate some memory */
  mtd_info = (struct mtd_info *)kmalloc(sizeof(struct mtd_info), GFP_KERNEL);
  if (!mtd_info)
    return -ENOMEM;

  addr = vmalloc(MTDCNXT_TOTAL_SIZE);
  if (!addr) {
    DEBUG(MTD_DEBUG_LEVEL1, 
          "Failed to vmalloc memory region of size %ld\n", 
          (long)MTDCNXT_TOTAL_SIZE);
    kfree(mtd_info);
    mtd_info = NULL;
    return -ENOMEM;
  }
  err = mtdcnxt_init_device(mtd_info, addr, 
                           MTDCNXT_TOTAL_SIZE, "mtdcnxt device");
  if (err) 
  {
    vfree(addr);
    kfree(mtd_info);
    mtd_info = NULL;
    return err;
  }
  memset(mtd_info->priv, 0xff, MTDCNXT_TOTAL_SIZE);
  return err;
}

module_init(init_mtdcnxt);
module_exit(cleanup_mtdcnxt);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alexander Larsson <alexl@redhat.com>");
MODULE_DESCRIPTION("Read/write-access to conexant controlled flash");
