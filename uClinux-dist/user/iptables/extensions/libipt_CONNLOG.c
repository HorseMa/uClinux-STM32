/* Shared library add-on to iptables to add CONNLOG target support. */
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <iptables.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter_ipv4/ipt_CONNLOG.h>

/* Function which prints out usage message. */
static void
help(void)
{
	printf(
"CONNLOG target v%s options:\n"
"  --confirm       Log confirm events\n"
"  --destroy       Log destroy events\n"
"\n",
IPTABLES_VERSION);
}

static struct option opts[] = {
	{ "confirm", 0, 0, '1' },
	{ "destroy", 0, 0, '2' },
	{ 0 }
};

/* Initialize the target. */
static void
init(struct ipt_entry_target *t, unsigned int *nfcache)
{
}

/* Function which parses command options; returns true if it
   ate an option */
static int
parse(int c, char **argv, int invert, unsigned int *flags,
      const struct ipt_entry *entry,
      struct ipt_entry_target **target)
{
	struct ipt_connlog_target_info *loginfo
		= (struct ipt_connlog_target_info *)(*target)->data;

	switch (c) {
	case '1':
		loginfo->events |= IPT_CONNLOG_CONFIRM;
		break;
	case '2':
		loginfo->events |= IPT_CONNLOG_DESTROY;
		break;
	default:
		return 0;
	}

	return 1;
}

static void
final_check(unsigned int flags)
{
}

/* Prints out the target info. */
static void
print(const struct ipt_ip *ip,
      const struct ipt_entry_target *target,
      int numeric)
{
	const struct ipt_connlog_target_info *loginfo =
		(const struct ipt_connlog_target_info *)target->data;

	printf("CONNLOG");
	if (loginfo->events & IPT_CONNLOG_CONFIRM)
		printf(" confirm");
	if (loginfo->events & IPT_CONNLOG_DESTROY)
		printf(" destroy");
}

/* Saves the target into in parsable form to stdout. */
static void
save(const struct ipt_ip *ip, const struct ipt_entry_target *target)
{
	const struct ipt_connlog_target_info *loginfo =
		(const struct ipt_connlog_target_info *)target->data;

	if (loginfo->events & IPT_CONNLOG_CONFIRM)
		printf("--confirm ");
	if (loginfo->events & IPT_CONNLOG_DESTROY)
		printf("--destroy ");
}

static struct iptables_target connlog_target = {
    .name          = "CONNLOG",
    .version       = IPTABLES_VERSION,
    .size          = IPT_ALIGN(sizeof(struct ipt_connlog_target_info)),
    .userspacesize = IPT_ALIGN(sizeof(struct ipt_connlog_target_info)),
    .help          = &help,
    .init          = &init,
    .parse         = &parse,
    .final_check   = &final_check,
    .print         = &print,
    .save          = &save,
    .extra_opts    = opts
};

void _init(void)
{
	register_target(&connlog_target);
}
