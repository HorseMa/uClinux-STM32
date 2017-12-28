/* Shared library add-on to iptables for authd. */
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <iptables.h>

/* Our less than helpful help
 */
static void authd_help(void)
{
	printf("authd v%s takes no options\n\n", IPTABLES_VERSION);
}

/* Initialize ourselves.
 */
static void authd_init(struct xt_entry_match *m)
{
}

/* Parse command options.
 * Since we have no options we never consume any and thus always
 * return false.
 */
static int authd_parse(int c, char **argv, int invert, unsigned int *flags,
		const void *entry, struct xt_entry_match **match)
{
	return 0;
}

/* Globals that contain our information.
 * We take no options so that structure is empty.
 */
static struct option authd_opts[] = {
	{ }
};

/* All of our information and work functions live here.
 */
static struct iptables_match authd_match = {
	.name		= "authd",
	.version	= IPTABLES_VERSION,
	.size		= IPT_ALIGN(0),
	.userspacesize	= IPT_ALIGN(0),
	.help		= &authd_help,
	.init		= &authd_init,
	.parse		= &authd_parse,
	.extra_opts	= authd_opts,
};

/* Our initialisation code.  Just register us as a target and that's it.
 * The kernel module and the user land authd process will take care of
 * everything.
 */
void _init(void)
{
	register_match(&authd_match);
}
