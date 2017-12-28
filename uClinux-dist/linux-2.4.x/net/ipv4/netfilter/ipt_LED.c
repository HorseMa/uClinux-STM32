/*
 * This is a module which is used for setting LEDs.
 */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/ledman.h>

#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter_ipv4/ipt_LED.h>
#include <linux/netfilter_ipv4/ip_conntrack.h>

#if 0
#define DEBUGP printk
#else
#define DEBUGP(format, args...)
#endif

static u_int32_t
get_led(struct sk_buff **pskb, const struct ipt_led_info *ledinfo)
{
#ifdef CONFIG_IP_NF_CONNTRACK
	enum ip_conntrack_info ctinfo;
	struct ip_conntrack *ct = ip_conntrack_get((*pskb), &ctinfo);
#endif

	switch (ledinfo->mode) {
	case IPT_LED_SAVE:
#ifdef CONFIG_IP_NF_CONNTRACK
		if (ct)
			ct->led = ledinfo->led;
#endif
		/* fallthrough */
	case IPT_LED_SET:
		return ledinfo->led;
	case IPT_LED_RESTORE:
#ifdef CONFIG_IP_NF_CONNTRACK
		if (ct)
			return ct->led;
#endif
		/* fallthrough */
	default:
		return LEDMAN_MAX;
	}
}

static unsigned int
ipt_led_target(struct sk_buff **pskb,
	       unsigned int hooknum,
	       const struct net_device *in,
	       const struct net_device *out,
	       const void *targinfo,
	       void *userinfo)
{
	const struct ipt_led_info *ledinfo = targinfo;
	u_int32_t led;

	led = get_led(pskb, ledinfo);
#ifdef CONFIG_LEDMAN
	if (led < LEDMAN_MAX)
		ledman_cmd(LEDMAN_CMD_SET, led);
#endif

	return IPT_CONTINUE;
}

static int ipt_led_checkentry(const char *tablename,
			      const struct ipt_entry *e,
			      void *targinfo,
			      unsigned int targinfosize,
			      unsigned int hook_mask)
{
	const struct ipt_led_info *ledinfo = targinfo;

	if (targinfosize != IPT_ALIGN(sizeof(struct ipt_led_info))) {
		DEBUGP("LED: targinfosize %u != %u\n",
		       targinfosize, IPT_ALIGN(sizeof(struct ipt_led_info)));
		return 0;
	}

	if (ledinfo->led >= LEDMAN_MAX) {
		DEBUGP("LED: led %u >= %u\n", ledinfo->led, LEDMAN_MAX);
		return 0;
	}

	return 1;
}

static struct ipt_target ipt_led_reg
= { { NULL, NULL }, "LED", ipt_led_target, ipt_led_checkentry, NULL, 
    THIS_MODULE };

static int __init init(void)
{
	if (ipt_register_target(&ipt_led_reg))
		return -EINVAL;
	
	return 0;
}

static void __exit fini(void)
{
	ipt_unregister_target(&ipt_led_reg);
}

module_init(init);
module_exit(fini);
MODULE_LICENSE("GPL");
