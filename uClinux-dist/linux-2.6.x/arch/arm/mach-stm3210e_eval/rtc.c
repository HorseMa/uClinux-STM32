#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/time.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/rtc.h>

#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/rtc.h>

#include <asm/arch/stm32f10x_conf.h>

#define DEBUG printk

extern int (*set_rtc)(void);

static int stm3210e_eval_set_rtc(void)
{
	DEBUG("%s\n",__FUNCTION__);
	RTC_SetCounter(xtime.tv_sec);
	return 1;
}

#define stm3210e_eval_read_alarm() ((unsigned long)(RTC->ALRH << 16 | RTC->ALRH ))


static int stm3210e_eval_rtc_read_alarm(struct rtc_wkalrm *alrm)
{
	rtc_time_to_tm(stm3210e_eval_read_alarm(), &alrm->time);
	return 0;
}

static inline int stm3210e_eval_rtc_set_alarm(struct rtc_wkalrm *alrm)
{
	unsigned long time;
	int ret;

	/*
	 * At the moment, we can only deal with non-wildcarded alarm times.
	 */
	ret = rtc_valid_tm(&alrm->time);
	if (ret == 0)
		ret = rtc_tm_to_time(&alrm->time, &time);
	if (ret == 0)
		RTC_SetAlarm(time);
	return ret;
}

static int stm3210e_eval_rtc_read_time(struct rtc_time *tm)
{
	DEBUG("%s\n",__FUNCTION__);
	rtc_time_to_tm(RTC_GetCounter(), tm);
	return 0;
}

/*
 * Set the RTC time.  Unfortunately, we can't accurately set
 * the point at which the counter updates.
 *
 * Also, since RTC_LR is transferred to RTC_CR on next rising
 * edge of the 1Hz clock, we must write the time one second
 * in advance.
 */
static inline int stm3210e_eval_rtc_set_time(struct rtc_time *tm)
{
	unsigned long time;
	int ret;

	ret = rtc_tm_to_time(tm, &time);
	if (ret == 0)
		RTC_SetCounter(time + 1);

	return ret;
}

static struct rtc_ops rtc_ops = {
	.owner		= THIS_MODULE,
	.read_time	= stm3210e_eval_rtc_read_time,
	.set_time	= stm3210e_eval_rtc_set_time,
	.read_alarm	= stm3210e_eval_rtc_read_alarm,
	.set_alarm	= stm3210e_eval_rtc_set_alarm,
};

static irqreturn_t stm32_rtc_interrupt(int irq, void *rtc)
{
	unsigned long		events = 0;


	/* alarm irq? */
	if (RTC_GetITStatus(RTC_IT_ALR)==SET) {
		RTC_ClearITPendingBit(RTC_IT_ALR);
		RTC_ClearFlag(RTC_FLAG_ALR);
		events |= RTC_IRQF | RTC_AF;
	}

	/* 1/sec periodic/update irq? */
	if (RTC_GetITStatus(RTC_IT_SEC)==SET )
	{
		RTC_ClearITPendingBit(RTC_IT_SEC);
		RTC_ClearFlag(RTC_FLAG_SEC);
		events |= RTC_IRQF | RTC_PF;
	}

	rtc_update_irq(rtc, 1,events);
	return IRQ_HANDLED;
}

static int rtc_probe(struct device *dev )
{
	int ret;

	xtime.tv_sec = RTC_GetCounter();

	/* note that 'dev' is merely used for irq disambiguation;
	 * it is not actually referenced in the irq handler
	 */
	ret = request_irq(RTC_IRQn, stm32_rtc_interrupt, IRQF_DISABLED,
			  "rtc-stm32f10x", dev);
	if (ret)
		goto out;

	ret = register_rtc(&rtc_ops);
	if (ret)
		goto irq_out;

	set_rtc = stm3210e_eval_set_rtc;
	return 0;

 irq_out:
	free_irq(RTC_IRQn, dev);
 out:
	return ret;
}

static int rtc_remove(struct device *dev)
{
	set_rtc = NULL;

	free_irq(RTC_IRQn, dev);
	unregister_rtc(&rtc_ops);

	return 0;
}


#define rtc_suspend 	NULL
#define  rtc_resume 	NULL



/*static struct device_driver rtc_driver = {
	
	.name	= "rtc-stm32f10x",
	.owner = THIS_MODULE,
	.probe		= rtc_probe,
	.remove		= rtc_remove,
	.suspend	= rtc_suspend,
	.resume		= rtc_resume,
};
*/
/*
static struct platform_device rtc_device = {
	.name	= "rtc-stm32f10x",
	.id	= 0,
};
*/
int rtc_driver_probe(struct platform_device *dev)
{
	DEBUG("rtc_driver_probe\n");
	rtc_probe(dev);
	return 0;
}
void rtc_driver_remove(struct platform_device *dev)
{
	DEBUG("rtc_driver_remove\n");
	rtc_remove(dev);
}

static struct platform_driver rtc_device_driver = {
	.probe   = rtc_driver_probe,
	.remove  = __devexit_p(rtc_driver_remove),
	.driver  = {
		.name	= "rtc-stm32f10x",
		.owner = THIS_MODULE,
		.probe		= rtc_probe,
		.remove		= rtc_remove,
		.suspend	= rtc_suspend,
		.resume		= rtc_resume,
	}
};

static int __init stm3210e_eval_rtc_init(void)
{
	int ret;
	printk("stm3210e_eval_rtc_init\n");
	ret=platform_driver_register(&rtc_device_driver);
	if(ret)
		printk("stm3210e_eval_rtc_init : error (%d)\n",ret);
	return ret;
}

static void __exit stm3210e_eval_rtc_exit(void)
{
	platform_driver_unregister(&rtc_device_driver);
}

module_init(stm3210e_eval_rtc_init);
module_exit(stm3210e_eval_rtc_exit);

/*
MODULE_AUTHOR("MCD Application Team");
MODULE_DESCRIPTION("RTC driver");
MODULE_LICENSE("GPL");
*/