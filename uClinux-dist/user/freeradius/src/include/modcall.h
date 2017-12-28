/* modcall.h: the outside interface to the module-calling tree. Includes
 * functions to build the tree from the config file, and to call it by
 * feeding it REQUESTs.
 *
 * Version: $Id: modcall.h,v 1.4 2004/02/26 19:04:19 aland Exp $ */

#include "conffile.h" /* Need CONF_* definitions */

/*
 *	For each authorize/authtype/etc, we have an ordered
 *	tree of instances to call.  This data structure keeps track
 *	of that order.
 */
typedef struct modcallable modcallable;

int modcall(int component, modcallable *c, REQUEST *request);

/* Parse a module-method's config section (e.g. authorize{}) into a tree that
 * may be called with modcall() */
modcallable *compile_modgroup(int component, CONF_SECTION *cs,
		const char *filename);

/* Create a single modcallable node that references a module instance. This
 * may be a CONF_SECTION containing action specifiers like "notfound = return"
 * or a simple CONF_PAIR, in which case the default actions are used. */
modcallable *compile_modsingle(int component, CONF_ITEM *ci,
		const char *filename, const char **modname);

/* Add an entry to the end of a modgroup, creating it first if necessary */
void add_to_modcallable(modcallable **parent, modcallable *this,
		int component, char *name);

/* Free a tree returned by compile_modgroup or compile_modsingle */
void modcallable_free(modcallable **pc);
