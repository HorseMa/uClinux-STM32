/*
 *  rlm_sql_log.c	Append the SQL queries in a log file which
 *			is read later by the radsqlrelay program
 *
 *  Version:    $Id: rlm_sql_log.c,v 1.3.2.2 2005/12/12 17:44:35 nbk Exp $
 *
 *  Author:     Nicolas Baradakis <nicolas.baradakis@cegetel.net>
 *
 *  Copyright (C) 2005 Cegetel
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#include "autoconf.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "libradius.h"
#include "radiusd.h"
#include "modules.h"
#include "conffile.h"

static const char rcsid[] = "$Id: rlm_sql_log.c,v 1.3.2.2 2005/12/12 17:44:35 nbk Exp $";

static int sql_log_instantiate(CONF_SECTION *conf, void **instance);
static int sql_log_detach(void *instance);
static int sql_log_accounting(void *instance, REQUEST *request);
static int sql_log_postauth(void *instance, REQUEST *request);

#define MAX_QUERY_LEN 4096

/*
 *	Define a structure for our module configuration.
 */
typedef struct rlm_sql_log_t {
	char		*name;
	char		*path;
	char		*postauth_query;
	char		*sql_user_name;
	char		*allowed_chars;
	CONF_SECTION	*conf_section;
} rlm_sql_log_t;

/*
 *	A mapping of configuration file names to internal variables.
 */
static const CONF_PARSER module_config[] = {
	{"path", PW_TYPE_STRING_PTR,
	 offsetof(rlm_sql_log_t,path), NULL, "${radacctdir}/sql-relay"},
	{"Post-Auth", PW_TYPE_STRING_PTR,
	 offsetof(rlm_sql_log_t,postauth_query), NULL, ""},
	{"sql_user_name", PW_TYPE_STRING_PTR,
	 offsetof(rlm_sql_log_t,sql_user_name), NULL, ""},
	{"safe-characters", PW_TYPE_STRING_PTR,
	 offsetof(rlm_sql_log_t,allowed_chars), NULL,
	"@abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-_: /"},

	{ NULL, -1, 0, NULL, NULL }	/* end the list */
};

static char *allowed_chars = NULL;

/*
 *	Do any per-module initialization that is separate to each
 *	configured instance of the module.  e.g. set up connections
 *	to external databases, read configuration files, set up
 *	dictionary entries, etc.
 *
 *	If configuration information is given in the config section
 *	that must be referenced in later calls, store a handle to it
 *	in *instance otherwise put a null pointer there.
 */
static int sql_log_instantiate(CONF_SECTION *conf, void **instance)
{
	const char	*name;
	rlm_sql_log_t	*inst;

        /*
         *      Set up a storage area for instance data.
         */
        inst = calloc(1, sizeof(rlm_sql_log_t));
        if (inst == NULL) {
                radlog(L_ERR, "rlm_sql_log: Not enough memory");
                return -1;
        }

	/*
	 *      Get the name of the current section in the conf file.
	 */
	name = cf_section_name2(conf);
	if (name == NULL)
		name = cf_section_name1(conf);
	if (name == NULL)
		name = "sql_log";
	inst->name = strdup(name);

	/*
	 *	If the configuration parameters can't be parsed,
	 *	then fail.
	 */
	if (cf_section_parse(conf, inst, module_config) < 0) {
		radlog(L_ERR, "rlm_sql_log (%s): Unable to parse parameters",
		       inst->name);
		sql_log_detach(inst);
		return -1;
	}

	inst->conf_section = conf;
	allowed_chars = inst->allowed_chars;
	*instance = inst;
	return 0;
}

/*
 *	Say goodbye to the cruel world.
 */
static int sql_log_detach(void *instance)
{
	int i;
	char **p;
	rlm_sql_log_t *inst = (rlm_sql_log_t *)instance;

	if (inst->name) {
		free(inst->name);
		inst->name = NULL;
	}

	/*
	 *	Free up dynamically allocated string pointers.
	 */
	for (i = 0; module_config[i].name != NULL; i++) {
		if (module_config[i].type != PW_TYPE_STRING_PTR) {
			continue;
		}

		/*
		 *	Treat 'config' as an opaque array of bytes,
		 *	and take the offset into it.  There's a
		 *      (char*) pointer at that offset, and we want
		 *	to point to it.
		 */
		p = (char **) (((char *)inst) + module_config[i].offset);
		if (!*p) { /* nothing allocated */
			continue;
		}
		free(*p);
		*p = NULL;
	}
	allowed_chars = NULL;
	free(inst);
	return 0;
}

/*
 *	Translate the SQL queries.
 */
static int sql_escape_func(char *out, int outlen, const char *in)
{
	int len = 0;

	while (in[0]) {
		/*
		 *	Non-printable characters get replaced with their
		 *	mime-encoded equivalents.
		 */
		if ((in[0] < 32) ||
		    strchr(allowed_chars, *in) == NULL) {
			/*
			 *	Only 3 or less bytes available.
			 */
			if (outlen <= 3) {
				break;
			}

			snprintf(out, outlen, "=%02X", (unsigned char) in[0]);
			in++;
			out += 3;
			outlen -= 3;
			len += 3;
			continue;
		}

		/*
		 *	Only one byte left.
		 */
		if (outlen <= 1) {
			break;
		}

		/*
		 *	Allowed character.
		 */
		*out = *in;
		out++;
		in++;
		outlen--;
		len++;
	}
	*out = '\0';
	return len;
}

/*
 *	Add the 'SQL-User-Name' attribute to the packet.
 */
static int sql_set_user(rlm_sql_log_t *inst, REQUEST *request, char *sqlusername, const char *username)
{
	VALUE_PAIR *vp=NULL;
	char tmpuser[MAX_STRING_LEN];

	tmpuser[0] = '\0';
	sqlusername[0] = '\0';

	/* Remove any user attr we added previously */
	pairdelete(&request->packet->vps, PW_SQL_USER_NAME);

	if (username != NULL) {
		strNcpy(tmpuser, username, MAX_STRING_LEN);
	} else if (inst->sql_user_name[0] != '\0') {
		radius_xlat(tmpuser, sizeof(tmpuser), inst->sql_user_name,
			    request, NULL);
	} else {
		return 0;
	}

	if (tmpuser[0] != '\0') {
		strNcpy(sqlusername, tmpuser, sizeof(tmpuser));
		DEBUG2("rlm_sql_log (%s): sql_set_user escaped user --> '%s'",
		       inst->name, sqlusername);
		vp = pairmake("SQL-User-Name", sqlusername, 0);
		if (vp == NULL) {
			radlog(L_ERR, "%s", librad_errstr);
			return -1;
		}

		pairadd(&request->packet->vps, vp);
		return 0;
	}
	return -1;
}

/*
 *	Replace %<whatever> in the query.
 */
static int sql_xlat_query(rlm_sql_log_t *inst, REQUEST *request, const char *query, char *xlat_query, size_t len)
{
	char	sqlusername[MAX_STRING_LEN];

	/* If query is not defined, we stop here */
	if (query[0] == '\0')
		return RLM_MODULE_NOOP;

	/* Add attribute 'SQL-User-Name' */
	if (sql_set_user(inst, request, sqlusername, NULL) <0) {
		radlog(L_ERR, "rlm_sql_log (%s): Couldn't add SQL-User-Name attribute",
			       inst->name);
		return RLM_MODULE_FAIL;
	}

	/* Expand variables in the query */
	xlat_query[0] = '\0';
	radius_xlat(xlat_query, len, query, request, sql_escape_func);
	if (xlat_query[0] == '\0') {
		radlog(L_ERR, "rlm_sql_log (%s): Couldn't xlat the query %s",
		       inst->name, query);
		return RLM_MODULE_FAIL;
	}

	return RLM_MODULE_OK;
}

/*
 *	The Perl version of radsqlrelay uses fcntl locks.
 */
static int setlock(int fd)
{
	struct flock fl;
	memset(&fl, 0, sizeof(fl));
	fl.l_start = 0;
	fl.l_len = 0;
	fl.l_type = F_WRLCK;
	fl.l_whence = SEEK_SET;
	return fcntl(fd, F_SETLKW, &fl);
}

/*
 *	Write the line into file (with lock)
 */
static int sql_log_write(rlm_sql_log_t *inst, REQUEST *request, const char *line)
{
	int fd;
	FILE *fp;
	int locked = 0;
	struct stat st;
	char path[MAX_STRING_LEN];

	path[0] = '\0';
	radius_xlat(path, sizeof(path), inst->path, request, NULL);
	if (path[0] == '\0')
		return RLM_MODULE_FAIL;

	while (!locked) {
		if ((fd = open(path, O_WRONLY | O_APPEND | O_CREAT, 0666)) < 0) {
			radlog(L_ERR, "rlm_sql_log (%s): Couldn't open file %s: %s",
			       inst->name, path, strerror(errno));
			return RLM_MODULE_FAIL;
		}
		if (setlock(fd) != 0) {
			radlog(L_ERR, "rlm_sql_log (%s): Couldn't lock file %s: %s",
			       inst->name, path, strerror(errno));
			close(fd);
			return RLM_MODULE_FAIL;
		}
		if (fstat(fd, &st) != 0) {
			radlog(L_ERR, "rlm_sql_log (%s): Couldn't stat file %s: %s",
			       inst->name, path, strerror(errno));
			close(fd);
			return RLM_MODULE_FAIL;
		}
		if (st.st_nlink == 0) {
			DEBUG("rlm_sql_log (%s): File %s removed by another program, retrying",
			      inst->name, path);
			close(fd);
			continue;
		}
		locked = 1;
	}

	if ((fp = fdopen(fd, "a")) == NULL) {
		radlog(L_ERR, "rlm_sql_log (%s): Couldn't associate a stream with file %s: %s",
		       inst->name, path, strerror(errno));
		close(fd);
		return RLM_MODULE_FAIL;
	}
	fputs(line, fp);
	putc('\n', fp);
	fclose(fp);	/* and unlock */
	return RLM_MODULE_OK;
}

/*
 *	Write accounting information to this module's database.
 */
static int sql_log_accounting(void *instance, REQUEST *request)
{
	int		ret;
	char		querystr[MAX_QUERY_LEN];
	char		*cfquery;
	rlm_sql_log_t	*inst = (rlm_sql_log_t *)instance;
	VALUE_PAIR	*pair;
	DICT_VALUE	*dval;
	CONF_PAIR	*cp;

	DEBUG("rlm_sql_log (%s): Processing sql_log_accounting", inst->name);

	/* Find the Acct Status Type. */
	if ((pair = pairfind(request->packet->vps, PW_ACCT_STATUS_TYPE)) == NULL) {
		radlog(L_ERR, "rlm_sql_log (%s): Packet has no account status type",
		       inst->name);
		return RLM_MODULE_INVALID;
	}

	/* Search the query in conf section of the module */
	if ((dval = dict_valbyattr(PW_ACCT_STATUS_TYPE, pair->lvalue)) == NULL) {
		radlog(L_ERR, "rlm_sql_log (%s): Unsupported Acct-Status-Type = %d",
		       inst->name, pair->lvalue);
		return RLM_MODULE_NOOP;
	}
	if ((cp = cf_pair_find(inst->conf_section, dval->name)) == NULL) {
		DEBUG("rlm_sql_log (%s): Couldn't find an entry %s in the config section",
		      inst->name, dval->name);
		return RLM_MODULE_NOOP;
	}
	cfquery = cf_pair_value(cp);

	/* Xlat the query */
	ret = sql_xlat_query(inst, request, cfquery, querystr, sizeof(querystr));
	if (ret != RLM_MODULE_OK)
		return ret;

	/* Write query into sql-relay file */
	return sql_log_write(inst, request, querystr);
}

/*
 *	Write post-auth information to this module's database.
 */
static int sql_log_postauth(void *instance, REQUEST *request)
{
	int		ret;
	char		querystr[MAX_QUERY_LEN];
	rlm_sql_log_t	*inst = (rlm_sql_log_t *)instance;

	DEBUG("rlm_sql_log (%s): Processing sql_log_postauth", inst->name);

	/* Xlat the query */
	ret = sql_xlat_query(inst, request, inst->postauth_query,
			     querystr, sizeof(querystr));
	if (ret != RLM_MODULE_OK)
		return ret;

	/* Write query into sql-relay file */
	return sql_log_write(inst, request, querystr);
}

/*
 *	The module name should be the only globally exported symbol.
 *	That is, everything else should be 'static'.
 *
 *	If the module needs to temporarily modify it's instantiation
 *	data, the type should be changed to RLM_TYPE_THREAD_UNSAFE.
 *	The server will then take care of ensuring that the module
 *	is single-threaded.
 */
module_t rlm_sql_log = {
	"sql_log",
	RLM_TYPE_THREAD_SAFE,		/* type */
	NULL,				/* initialization */
	sql_log_instantiate,		/* instantiation */
	{
		NULL,			/* authentication */
		NULL,			/* authorization */
		NULL,			/* preaccounting */
		sql_log_accounting,	/* accounting */
		NULL,			/* checksimul */
		NULL,			/* pre-proxy */
		NULL,			/* post-proxy */
		sql_log_postauth	/* post-auth */
	},
	sql_log_detach,			/* detach */
	NULL,				/* destroy */
};
