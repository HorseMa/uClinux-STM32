/* Optionally log start and end of connections.
 */
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>

#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter_ipv4/ipt_CONNLOG.h>
#include <linux/netfilter_ipv4/ip_conntrack.h>
#include <linux/netfilter_ipv4/ip_conntrack_protocol.h>
#include <linux/netfilter_ipv4/ip_conntrack_core.h>

/* Use lock to serialize, so printks don't overlap */
static spinlock_t log_lock = SPIN_LOCK_UNLOCKED;

static unsigned int
target(struct sk_buff **pskb,
       unsigned int hooknum,
       const struct net_device *in,
       const struct net_device *out,
       const void *targinfo,
       void *userinfo)
{
	const struct ipt_connlog_target_info *loginfo = targinfo;
	enum ip_conntrack_info ctinfo;
	struct ip_conntrack *ct = ip_conntrack_get((*pskb), &ctinfo);

	if (ct) {
		if (!loginfo->events || (loginfo->events & IPT_CONNLOG_CONFIRM))
			set_bit(IPS_LOG_CONFIRM_BIT, &ct->status);
		if (!loginfo->events || (loginfo->events & IPT_CONNLOG_DESTROY))
			set_bit(IPS_LOG_DESTROY_BIT, &ct->status);
	}

	return IPT_CONTINUE;
}

static int
checkentry(const char *tablename,
	   const struct ipt_entry *e,
           void *targinfo,
           unsigned int targinfosize,
           unsigned int hook_mask)
{
	if (targinfosize != IPT_ALIGN(sizeof(struct ipt_connlog_target_info))) {
		printk(KERN_WARNING "CONNLOG: targinfosize %u != %Zu\n",
		       targinfosize,
		       IPT_ALIGN(sizeof(struct ipt_connlog_target_info)));
		return 0;
	}

	return 1;
}

static struct ipt_target ipt_connlog_reg = {
	.name = "CONNLOG",
	.target = target,
	.checkentry = checkentry,
	.me = THIS_MODULE,
};

static void dump_tuple(char *buffer, const struct ip_conntrack_tuple *tuple,
		       struct ip_conntrack_protocol *proto,
		       const char *prefix)
{
	char *p, *q;

	printk("%ssrc=%u.%u.%u.%u %sdst=%u.%u.%u.%u ",
		prefix, NIPQUAD(tuple->src.ip),
		prefix, NIPQUAD(tuple->dst.ip));

	if (proto->print_tuple(buffer, tuple)) {
		for (p = q = buffer; *p; p++) {
			if (*p == ' ') {
				*p = 0;
				if (*q)
					printk("%s%s ", prefix, q);
				q = p + 1;
			}

		}
	}
}

#ifdef CONFIG_IP_NF_CT_ACCT
static void dump_counters(struct ip_conntrack_counter *counter,
		const char *prefix)
{
	printk("%spackets=%llu %sbytes=%llu ", prefix, counter->packets,
			prefix, counter->bytes);
}
#else
static inline void dump_counters(struct ip_conntrack_counter *counter) {}
#endif

static void dump_conntrack(struct ip_conntrack *ct)
{
	struct ip_conntrack_tuple *orig_t
		= &ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple;
	struct ip_conntrack_tuple *reply_t
		= &ct->tuplehash[IP_CT_DIR_REPLY].tuple;
	struct ip_conntrack_protocol *proto
		= __ip_ct_find_proto(orig_t->dst.protonum);
	char buffer[256];

	printk("id=%p ", ct);
	printk("proto=%s ", proto->name);

	if (proto->print_conntrack(buffer, ct))
		printk("%s", buffer);

#ifdef CONFIG_IP_NF_CT_DEV
	printk("in=%s out=%s ", ct->indev, ct->outdev);
#endif

	dump_tuple(buffer, orig_t, proto, "tx-");
	dump_counters(&ct->counters[IP_CT_DIR_ORIGINAL], "tx-");

	dump_tuple(buffer, reply_t, proto, "rx-");
	dump_counters(&ct->counters[IP_CT_DIR_REPLY], "rx-");

#if defined(CONFIG_IP_NF_CONNTRACK_MARK)
	printk("mark=%ld ", ct->mark);
#endif
}

static int conntrack_event(struct notifier_block *this,
			   unsigned long event,
			   void *ptr)
{
	struct ip_conntrack *ct = ptr;
	const char *log_prefix;

	if ((event & (IPCT_NEW|IPCT_RELATED)) &&
	    test_bit(IPS_LOG_CONFIRM_BIT, &ct->status))
		log_prefix = "create";
	else if ((event & (IPCT_DESTROY)) &&
	    test_bit(IPS_LOG_DESTROY_BIT, &ct->status))
		log_prefix = "destroy";
	else
		goto done;

	spin_lock_bh(&log_lock);
	printk(KERN_INFO "conntrack %s: ", log_prefix);
	dump_conntrack(ct);
	printk("\n");
	spin_unlock_bh(&log_lock);

done:
	return NOTIFY_DONE;
}

static struct notifier_block conntrack_notifier = {
	.notifier_call = conntrack_event,
};

static int __init init(void)
{
	int ret;

	ret = ipt_register_target(&ipt_connlog_reg);
	if (ret != 0)
		return ret;

	ret = ip_conntrack_register_notifier(&conntrack_notifier);
	if (ret != 0)
		goto unregister_connlog;

	return ret;

unregister_connlog:
	ipt_unregister_target(&ipt_connlog_reg);

	return ret;
}

static void __exit fini(void)
{
	ipt_unregister_target(&ipt_connlog_reg);
	ip_conntrack_unregister_notifier(&conntrack_notifier);
}

module_init(init);
module_exit(fini);
