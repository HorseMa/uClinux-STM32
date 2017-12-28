/*
 * An RTC test device/driver
 * Copyright (C) 2005 Tower Technologies
 * Author: Alessandro Zummo <a.zummo@towertech.it>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/err.h>
#include <linux/rtc.h>
#include <linux/bcd.h>
#include <linux/platform_device.h>
#include <asm/arch/stm32f10x_conf.h>

#define DBG(fmt, args...) pr_debug("%s:%i: " fmt "\n", __func__, __LINE__, ## args)

static struct platform_device *stm32_rtc_device = NULL;

#define stm3210e_eval_read_alarm() ((unsigned long)(RTC->ALRH << 16 | RTC->ALRH ))

static int stm32_rtc_read_alarm(struct device *dev,
	struct rtc_wkalrm *alrm)
{
	DBG();
	rtc_time_to_tm(stm3210e_eval_read_alarm() , &alrm->time);
	return 0;
}

static int stm32_rtc_set_alarm(struct device *dev,
	struct rtc_wkalrm *alrm)
{
	struct rtc_time tm = alrm->time;
	if(alrm->enabled)
		RTC_SetAlarm((tm.tm_min*60)+(tm.tm_hour*3600)+(tm.tm_sec));
	return 0;
}

static int stm32_rtc_read_time(struct device *dev,struct rtc_time *tm)
{
	local_irq_disable();
	rtc_time_to_tm(RTC_GetCounter() + (BKP_ReadBackupRegister(BKP_DR1) * 0xffffffff), tm);	
	local_irq_enable();
	return 0;
}

static int stm32_rtc_set_time(struct device *dev,struct rtc_time *tm)
{
	unsigned long time;
	int ret;
	ret = rtc_tm_to_time(tm, &time);
	if (ret == 0)
	{
		BKP_WriteBackupRegister(BKP_DR1,(time / 0xffffffff));
		RTC_SetCounter((time % 0xffffffff ) + 1);
	}

	return ret;
}

static int stm32_rtc_ioctl(struct device *dev, unsigned int cmd,unsigned long arg)
{
	static struct rtc_time *p_tm ;
	
	DBG("cmd 0x%x",cmd);
	
	switch (cmd) {

	case RTC_SET_TIME:
		p_tm = (struct rtc_time*)arg;
		stm32_rtc_set_time(dev,p_tm);
		return 0;

	case RTC_RD_TIME:
	{
		p_tm = (struct rtc_time*)arg;
		stm32_rtc_read_time(dev,p_tm);
		return 0;		
	}
	case RTC_UIE_ON:
	case RTC_PIE_ON	:
		DBG("RTC_UIE_ON");
		RTC_ITConfig(RTC_IT_SEC, ENABLE);
		return 0;
	case RTC_UIE_OFF:
	case RTC_PIE_OFF:
		DBG("RTC_UIE_OFF");
		RTC_ITConfig(RTC_IT_SEC, DISABLE);
		return 0;
	case RTC_AIE_ON:
		DBG("RTC_AIE_ON");
		RTC_ITConfig(RTC_IT_ALR, ENABLE);
		return 0;
	case RTC_AIE_OFF:
		DBG("RTC_AIE_OFF");
		RTC_ITConfig(RTC_IT_ALR, ENABLE);
		return 0;

	default:
		DBG();
		return -ENOIOCTLCMD;
	}
}

static const struct rtc_class_ops stm32_rtc_ops = {
	.read_time = stm32_rtc_read_time,
	.set_time = stm32_rtc_set_time,
	.read_alarm = stm32_rtc_read_alarm,
	.set_alarm = stm32_rtc_set_alarm,
	.ioctl = stm32_rtc_ioctl,
};

static irqreturn_t rtc_irq(int irq, void *rtc)
{
	unsigned long		events = 0;


	/* alarm irq? */
	/*if (RTC_GetITStatus(RTC_IT_ALR) == SET) {
		RTC_ClearITPendingBit(RTC_IT_ALR);
		events |= RTC_IRQF | RTC_AF;
	}*/

	/* 1/sec periodic/update irq? */
	if (RTC_GetITStatus(RTC_IT_SEC) == SET )
	{
		events |= RTC_IRQF | RTC_PF;
		RTC_ClearITPendingBit(RTC_IT_SEC);
	}
	
	/* overflow irq? */
	if (RTC_GetITStatus(RTC_IT_OW) == SET )
	{
		BKP_WriteBackupRegister(BKP_DR1, BKP_ReadBackupRegister(BKP_DR1) + 1);
		RTC_SetCounter(1);
		RTC_ClearITPendingBit(RTC_IT_OW);
	}

	rtc_update_irq(rtc, 1, events);

	return IRQ_HANDLED;
}

void RTC_Configuration(void)
{
  /* Enable PWR and BKP clocks */
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);

  /* Allow access to BKP Domain */
  PWR_BackupAccessCmd(ENABLE);

  /* Reset Backup Domain */
  //BKP_DeInit();
  
  
 #if 0
 
  /* Enable LSI */
  RCC_LSICmd(ENABLE);
  /* Wait till LSE is ready */
  while (RCC_GetFlagStatus(RCC_FLAG_LSIRDY) == RESET)
  {}

  /* Select LSI as RTC Clock Source */
  RCC_RTCCLKConfig(RCC_RTCCLKSource_LSI);
  
#else

  /* Enable LSE */
  RCC_LSEConfig(RCC_LSE_ON);
  /* Wait till LSE is ready */
  while (RCC_GetFlagStatus(RCC_FLAG_LSERDY) == RESET)
  {}

  /* Select LSE as RTC Clock Source */
  RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE);
#endif  
  
  /* Enable RTC Clock */
  RCC_RTCCLKCmd(ENABLE);

  /* Wait for RTC registers synchronization */
  RTC_WaitForSynchro();

  /* Wait until last write operation on RTC registers has finished */
  RTC_WaitForLastTask();

  /* Enable the RTC Second */
  RTC_ITConfig(RTC_IT_SEC, ENABLE);
  RTC_ITConfig(RTC_IT_OW, ENABLE);
  //NVIC_EnableIRQ(RTC_IT_SEC);
  
  /* Wait until last write operation on RTC registers has finished */
  RTC_WaitForLastTask();

  /* Set RTC prescaler: set RTC period to 1sec */
  RTC_SetPrescaler(32767); /* RTC period = RTCCLK/RTC_PR = (32.768 KHz)/(32767+1) */

  /* Wait until last write operation on RTC registers has finished */
  RTC_WaitForLastTask();
}

static int stm32_probe(struct platform_device *plat_dev)
{
	int err;
	struct rtc_device *rtc = rtc_device_register("rtc-stm3210e_eval", &plat_dev->dev,
						&stm32_rtc_ops, THIS_MODULE);
	if (IS_ERR(rtc)) {
		err = PTR_ERR(rtc);
		return err;
	}
	err = request_irq(RTC_IRQn, rtc_irq, IRQF_DISABLED,
			rtc->dev.bus_id, rtc);
	if (err)
		goto err;

	platform_set_drvdata(plat_dev, rtc);
	return 0;

err:
	rtc_device_unregister(rtc);
	return err;
}

static int __devexit stm32_remove(struct platform_device *plat_dev)
{
	struct rtc_device *rtc = platform_get_drvdata(plat_dev);
	
	free_irq(RTC_IRQn, NULL);
	rtc_device_unregister(rtc);

	return 0;
}

static struct platform_driver stm32_driver = {
	.probe	= stm32_probe,
	.remove = __devexit_p(stm32_remove),
	.driver = {
		.name = "rtc-stm3210e_eval",
		.owner = THIS_MODULE,
	},
};

static int __init stm32_init(void)
{
	int err;
	
	RTC_Configuration();
	
	if ((err = platform_driver_register(&stm32_driver)))
		return err;

	if ((stm32_rtc_device = platform_device_alloc("rtc-stm3210e_eval", 0)) == NULL) {
		err = -ENOMEM;
		goto exit_driver_unregister;
	}
	
	if ((err = platform_device_add(stm32_rtc_device)))
		goto exit_driver_unregister;
			
	return 0;

exit_driver_unregister:
	platform_driver_unregister(&stm32_driver);
	return err;
}

static void __exit stm32_exit(void)
{
	platform_device_unregister(stm32_rtc_device);
	platform_driver_unregister(&stm32_driver);
}

MODULE_AUTHOR("MCD Application Team");
MODULE_DESCRIPTION("RTC test driver/device");
MODULE_LICENSE("GPL");

module_init(stm32_init);
module_exit(stm32_exit);
