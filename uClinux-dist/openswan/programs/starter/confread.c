/* Openswan config file parser (confread.c)
 * Copyright (C) 2001-2002 Mathieu Lafon - Arkoon Network Security
 * Copyright (C) 2004 Xelerance Corporation
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.  See <http://www.fsf.org/copyleft/gpl.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * RCSID $Id: confread.c,v 1.11 2004-04-11 15:17:30 ken Exp $
 */

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include <sys/queue.h>

#include "parser.h"
#include "confread.h"
#include "interfaces.h"
#include "starterlog.h"

static char _tmp_err[512];

/** 
 * A policy only conn means that we load it, and do the appropriate firewalling to make
 * sure that no packets get out that this conn would apply to, but we refuse to negotiate
 * it in any way, either incoming or outgoing.
 */
#define POLICY_ONLY_CONN(conn) if(conn->options[KBF_AUTO] > STARTUP_ROUTE) { conn->options[KBF_AUTO]=STARTUP_POLICY; }

void free_list(char **list);
char **new_list(char *value);


/** 
 * Set up hardcoded defaults, from data in programs/pluto/constants.h
 *
 * @param cfg starter_config struct
 * @return void
 */
static void default_values (struct starter_config *cfg)
{
	if (!cfg) return;
	memset(cfg, 0, sizeof(struct starter_config));

	TAILQ_INIT(&cfg->conns);

	cfg->setup.options[KBF_FRAGICMP] = TRUE;
	cfg->setup.options[KBF_HIDETOS]  = TRUE;
	cfg->setup.options[KBF_UNIQUEIDS]= FALSE;
	cfg->setup.options[KBF_TYPE] = KS_TUNNEL;

	cfg->conn_default.policy = POLICY_ENCRYPT | POLICY_TUNNEL | POLICY_RSASIG;

	cfg->conn_default.options[KBF_IKELIFETIME] = OAKLEY_ISAKMP_SA_LIFETIME_DEFAULT;
	cfg->conn_default.options[KBF_SALIFETIME]  = SA_LIFE_DURATION_DEFAULT;
	cfg->conn_default.options[KBF_REKEYMARGIN] = SA_REPLACEMENT_MARGIN_DEFAULT;
	cfg->conn_default.options[KBF_REKEYFUZZ]   = SA_REPLACEMENT_FUZZ_DEFAULT;
	cfg->conn_default.options[KBF_KEYINGTRIES] = SA_REPLACEMENT_RETRIES_DEFAULT;

	cfg->conn_default.left.addr_family = AF_INET;
	anyaddr(AF_INET, &cfg->conn_default.left.addr);
	anyaddr(AF_INET, &cfg->conn_default.left.nexthop);
	cfg->conn_default.right.addr_family = AF_INET;
	anyaddr(AF_INET, &cfg->conn_default.right.addr);
	anyaddr(AF_INET, &cfg->conn_default.right.nexthop);

	cfg->conn_default.options[KBF_AUTO] = STARTUP_NO;
	cfg->conn_default.state = STATE_LOADED;
}

#define ERR_FOUND(args...) \
	{ if (perr && (*perr==NULL)) { \
		snprintf(_tmp_err, sizeof(_tmp_err)-1, ## args); \
		*perr = xstrdup(_tmp_err); } \
	err++; }

#define KW_POLICY_FLAG(val,fl) if(conn->options_set[val]) \
        { if(conn->options[val]) \
	  { \
	    conn->policy |= fl; \
	  } else { \
	    conn->policy &= ~fl;\
	  }}

/**
 * Free the pointer list
 *
 * @param list list of pointers
 * @return void
 */
void free_list(char **list)
{
	char **s;
	for (s=list; *s; s++) free(*s);
	free(list);
}


/**
 * Create a new list of pointers
 *
 * @param value 
 * @return new_list (pointer to list of pointers)
 */
char **new_list(char *value)
{
	char *val, *b, *e, *end, **ret;
	int count;
	val = value ? xstrdup(value) : NULL;
	if (!val) return NULL;
	end = val + strlen(val);
	for (b=val, count=0; b<end; ) {
		for (e=b; ((*e!=' ') && (*e!='\0')); e++);
		*e = '\0';
		if (e!=b) { count++; }
		b=e+1;
	}
	if (count==0) {
		free(val);
		return NULL;
	}
	ret = (char **)malloc((count+1) * sizeof(char *));
	if (!ret) {
		free(val);
		return NULL;
	}
	for (b=val, count=0; b<end; ) {
		for (e=b; (*e!='\0'); e++);
		if (e!=b) {
			ret[count++] = xstrdup(b);
		}
		b=e+1;
	}
	ret[count] = NULL;
	free(val);
	return ret;
}

/**
 * Load a parsed config 
 *
 * @param cfg starter_config structure
 * @param cfgp config_parsed (ie: valid) struct
 * @param perr pointer to store errors in
 * @return int 0 if successfull
 */
static int load_setup (struct starter_config *cfg
		       , struct config_parsed *cfgp
		       , char **perr)
{
	unsigned int err = 0;
	struct kw_list *kw;

	for (kw=cfgp->config_setup; kw; kw=kw->next) {

	    /**
	     * the parser already made sure that only config keywords were used,
	     * but we double check!
	     */
	    assert(kw->keyword.keydef->validity & kv_config);

	    switch(kw->keyword.keydef->type)
	    {
	    case kt_string:
	    case kt_filename:
	    case kt_dirname:
	    case kt_loose_enum:
		/* all treated as strings for now */
		assert(kw->keyword.keydef->field < sizeof(cfg->setup.strings));
		if(cfg->setup.strings[kw->keyword.keydef->field]) free(cfg->setup.strings[kw->keyword.keydef->field]);
		cfg->setup.strings[kw->keyword.keydef->field] = xstrdup(kw->string);
		break;

	    case kt_appendstring:
		assert(kw->keyword.keydef->field < KEY_STRINGS_MAX);
		if(!cfg->setup.strings[kw->keyword.keydef->field])
		{
		    cfg->setup.strings[kw->keyword.keydef->field] = xstrdup(kw->string);
		} else {
		    int len;
		    char *s;
		    
		    len = strlen(cfg->setup.strings[kw->keyword.keydef->field])+1;
		    len += strlen(kw->string)+1;
		    
		    /* allocate the string */
		    s = cfg->setup.strings[kw->keyword.keydef->field];
		    s = xrealloc(s, len);
		    strncat(s, " ", len);
		    strncat(s, kw->string, len);
		    
		    cfg->setup.strings[kw->keyword.keydef->field] = s;
		}
		break;
		
	    case kt_list:
	    case kt_bool:
	    case kt_invertbool:
	    case kt_enum:
	    case kt_number:
	    case kt_time:
	    case kt_percent:
		/* all treated as a number for now */
		assert(kw->keyword.keydef->field < sizeof(cfg->setup.options));
		cfg->setup.options[kw->keyword.keydef->field] = kw->number;
		break;

	    case kt_bitstring:
	    case kt_rsakey:
	    case kt_ipaddr:
	    case kt_subnet:
	    case kt_idtype:
		err++;
		break;
	    }
	}
		
	/* now process some things with specific values */
	
	/* interfaces has to be chopped up */
	if (cfg->setup.interfaces) free_list(cfg->setup.interfaces);
	cfg->setup.interfaces = new_list(cfg->setup.strings[KSF_INTERFACES]);

	return err;
}

/**
 * Validate that yes in fact we are one side of the tunnel
 * 
 * The function checks that IP addresses are valid, nexthops are
 * present (if needed) as well as policies
 *
 * @param conn_st a connection definition
 * @param end a connection end
 * @param left boolean (are we 'left'? 1 = yes, 0 = no)
 * @param perr pointer to char containing error value
 * @return int 0 if successfull
 */
static int validate_end(struct starter_conn *conn_st
			, struct starter_end *end
			, bool left, char **perr)
{
    err_t er = NULL;
    int err=0;
    
    end->addrtype=end->options[KNCF_IP];

    /* validate the KSCF_IP/KNCF_IP */
    switch(end->addrtype)
    {
    case KH_ANY:
	anyaddr(AF_INET, &(end->addr));
	break;

    case KH_IFACE:
	assert(end->strings[KSCF_IP] != NULL);

	if (end->iface) free(end->iface);
	end->iface = xstrdup(end->strings[KNCF_IP]);
	if (starter_iface_find(end->iface, AF_INET, &(end->addr),
			       &(end->nexthop)) == -1) {
	    conn_st->state = STATE_INVALID;
	}
	break;
	
    case KH_IPADDR:
	assert(end->strings[KSCF_IP] != NULL);

	er = ttoaddr(end->strings[KNCF_IP], 0, AF_INET, &(end->addr));
	if (er) ERR_FOUND("bad addr %s=%s [%s]", (left ? "left" : "right"), end->strings[KNCF_IP], er);
	break;
	
    case KH_OPPO:
	conn_st->policy |= POLICY_OPPO;
	break;

    case KH_OPPOGROUP:
	conn_st->policy |= POLICY_GROUP|POLICY_GROUP;
	break;

    case KH_GROUP:
	conn_st->policy |= POLICY_GROUP;
	break;
	
    case KH_DEFAULTROUTE:
	break;

    case KH_NOTSET:
	break;
    }

    /* validate the KSCF_SUBNET */
    if(end->strings[KSCF_SUBNET] != NULL)
    {
	char *value = end->strings[KSCF_SUBNET];

#ifdef VIRTUAL_IP
        if ( ((strlen(value)>=6) && (strncmp(value,"vhost:",6)==0)) ||
	     ((strlen(value)>=5) && (strncmp(value,"vnet:",5)==0)) ) {
	    er = NULL;
	    end->virt = xstrdup(value);
	    if (!end->virt) ERR_FOUND("can't alloc memory");
	}
	else {
	    end->has_client = TRUE;
	    er = ttosubnet(value, 0, AF_INET, &(end->subnet));
	}
#else
	end->has_client = TRUE;
	end->has_client_wildcard = FALSE;
	er = ttosubnet(value, 0, AF_INET, &(end->subnet));
#endif
	if (er) ERR_FOUND("bad subnet %s=%s [%s]", (left ? "leftsubnet" : "rightsubnet"), value, er);
    }

    /* validate the KSCF_NEXTHOP */
    if(end->strings[KSCF_NEXTHOP] != NULL)
    {
	char *value = end->strings[KSCF_NEXTHOP];
	
	er = ttoaddr(value, 0, AF_INET, &(end->nexthop));
	if (er) ERR_FOUND("bad addr %s=%s [%s]", (left ? "lextnexthop" : "rightnexthop"), value, er);
    } else {
	anyaddr(AF_INET, &end->nexthop);
    }

    /* validate the KSCF_ID */
    if(end->strings[KSCF_ID] != NULL)
    {
	char *value = end->strings[KSCF_ID];
	
	if (end->id) free(end->id);
	end->id = xstrdup(value);
    }

    /* validate the KSCF_RSAKEY1/RSAKEY2 */
    if(end->strings[KSCF_RSAKEY1] != NULL)
    {
	char *value = end->strings[KSCF_RSAKEY1];

	if (end->rsakey1) free(end->rsakey1);
	end->rsakey1 = xstrdup(value);
    }
    if(end->strings[KSCF_RSAKEY2] != NULL)
    {
	char *value = end->strings[KSCF_RSAKEY2];

	if (end->rsakey2) free(end->rsakey2);
	end->rsakey2 = xstrdup(value);
    }

    return err;
}


/**
 * Take keywords from ipsec.conf syntax and load into a conn struct
 * 
 *
 * @param conn a connection definition
 * @param sl a section_list
 * @param permitreplace boolean (can we replace this conn?)
 * @return bool 0 if successfull
 */
static bool translate_conn (struct starter_conn *conn
			    , struct section_list *sl
			    , bool permitreplace)
{
    unsigned int err, field;
    ksf    *the_strings;
    knf    *the_options;
    str_set *set_strings;
    int_set *set_options;
    volatile int i;              /* just to keep it around for debugging */
    struct kw_list *kw = sl->kw;
	
    err = 0;
    i = 0;

    for ( ; kw; kw=kw->next)
    {
	i++;
	the_strings = &conn->strings;
	set_strings = &conn->strings_set;
	the_options = &conn->options;
	set_options = &conn->options_set;
	
	if((kw->keyword.keydef->validity & kv_conn) == 0)
	{
	    /* this isn't valid in a conn! */
	    starter_log(LOG_LEVEL_INFO,
			"keyword '%s' is not valid in a conn (%s) (#%d)\n",
			kw->keyword.keydef->keyname, sl->name, i);
	    continue;
	}
	
	if(kw->keyword.keydef->validity & kv_leftright)
	{
	    if(kw->keyword.keyleft)
	    {
		the_strings = &conn->left.strings;
		the_options = &conn->left.options;
		set_strings = &conn->left.strings_set;
		set_options = &conn->left.options_set;
	    } else {
		the_strings = &conn->right.strings;
		the_options = &conn->right.options;
		set_strings = &conn->right.strings_set;
		set_options = &conn->right.options_set;
	    }
	}
	
	field = kw->keyword.keydef->field;

	assert(kw->keyword.keydef != NULL);
	switch(kw->keyword.keydef->type)
	{
	case kt_string:
	case kt_filename:
	case kt_dirname:
	case kt_bitstring:
	case kt_ipaddr:
	case kt_subnet:
	case kt_idtype:
	    /* all treated as strings for now */
	    assert(kw->keyword.keydef->field < KEY_STRINGS_MAX);
	    if((*set_strings)[field])
	    {
		if(!permitreplace)
		{
		    starter_log(LOG_LEVEL_INFO
				, "duplicate key '%s' in conn %s while processing def %s"
				, kw->keyword.keydef->keyname
				, conn->name
				, sl->name);
		    if(kw->keyword.string == NULL
		       || (*the_strings)[field] == NULL
		       || strcmp(kw->keyword.string, (*the_strings)[field])!=0)
		    {
			err++;
			break;
		    }
		}
	    }
	    if((*the_strings)[field])
	    {
		    free((*the_strings)[field]);
	    }
	    
	    (*the_strings)[field] = xstrdup(kw->string);
	    (*set_strings)[field] = TRUE;
	    break;
	    
	case kt_appendstring:
	    /* implicitely, this field can have multiple values */
	    assert(kw->keyword.keydef->field < KEY_STRINGS_MAX);
	    if(!(*the_strings)[field])
	    {
		(*the_strings)[field] = xstrdup(kw->string);
	    } else {
		int len;
		char *s;
		
		len = strlen((*the_strings)[field])+1;
		len += strlen(kw->string)+1;
		
		/* allocate the string */
		s = (*the_strings)[field];
		s = xrealloc(s, len);
		strncat(s, " ", len);
		strncat(s, kw->string, len);
		
		(*the_strings)[field] = s;
	    }
	    (*set_strings)[field] = TRUE;
	    break;
	    
	case kt_rsakey:
	case kt_loose_enum:
	    assert(field < KEY_STRINGS_MAX);
	    assert(field < KEY_NUMERIC_MAX);
	    
	    if((*set_options)[field])
	    {
		if(!permitreplace)
		{
		    starter_log(LOG_LEVEL_INFO
				, "duplicate key '%s' in conn %s while processing def %s"
				, kw->keyword.keydef->keyname
				, conn->name
				, sl->name);

		    /* only fatal if we try to change values */
		    if((*the_options)[field] != kw->number
		       || !((*the_options)[field] == LOOSE_ENUM_OTHER
			    && kw->number == LOOSE_ENUM_OTHER
			    && kw->keyword.string != NULL
			    && (*the_strings)[field] != NULL
			    && strcmp(kw->keyword.string, (*the_strings)[field])==0))
		    {
			err++;
			break;
		    }
		}
	    }

	    (*the_options)[field] = kw->number;
	    if(kw->number == LOOSE_ENUM_OTHER)
	    {
		assert(kw->keyword.string != NULL);
		if((*the_strings)[field]) free((*the_strings)[field]);
		(*the_strings)[field] = xstrdup(kw->keyword.string);
	    } 
	    (*set_options)[field] = TRUE;
	    break;
	    
	case kt_list:
	case kt_bool:
	case kt_invertbool:
	case kt_enum:
	case kt_number:
	case kt_time:
	case kt_percent:
	    /* all treated as a number for now */
	    assert(field < KEY_NUMERIC_MAX);

	    if((*set_options)[field])
	    {
		if(!permitreplace)
		{
		    starter_log(LOG_LEVEL_INFO
				, "duplicate key '%s' in conn %s while processing def %s"
				, kw->keyword.keydef->keyname
				, conn->name
				, sl->name);
		    if((*the_options)[field] != kw->number)
		    {
			err++;
			break;
		    }
		}
	    }

	    (*the_options)[field] = kw->number;
	    (*set_options)[field] = TRUE;
	    break;
	}
    }
    return err;
}


static int load_conn_basic(struct starter_conn *conn
			   , struct section_list *sl)
{
    int err;

    memset(conn->strings_set, 0, sizeof(conn->strings_set));
    memset(conn->options_set, 0, sizeof(conn->options_set));
    memset(conn->left.strings_set, 0, sizeof(conn->left.strings_set));
    memset(conn->left.options_set, 0, sizeof(conn->left.options_set));
    memset(conn->right.strings_set, 0, sizeof(conn->left.strings_set));
    memset(conn->right.options_set, 0, sizeof(conn->left.options_set));

    /* turn all of the keyword/value pairs into options/strings in left/right */
    err = translate_conn(conn, sl, TRUE);

    /* now, process the also's */
    if (conn->alsos) free_list(conn->alsos);
    conn->alsos = new_list(conn->strings[KSF_ALSO]);

    return err;
}



static int load_conn (struct starter_config *cfg
		      , struct starter_conn *conn
		      , struct config_parsed *cfgp
		      , struct section_list *sl
		      , bool alsoprocessing
		      , char **perr)
{
    unsigned int err;
    char **alsos;
    char **newalsos;
    int   newalsoplace;
    int   alsoplace;
    int   alsosize;
    struct section_list *sl1;
	
    err = 0;

    err += load_conn_basic(conn, sl);
    if(err) return err;

    if(conn->strings[KSF_ALSO] != NULL
       && !alsoprocessing)
    {
	starter_log(LOG_LEVEL_INFO
		    , "also= is not valid in section '%s'"
		    , sl->name);
	return 1;
    }

    /* now, process the also's */
    if (conn->alsos) free_list(conn->alsos);
    conn->alsos = new_list(conn->strings[KSF_ALSO]);

    if(alsoprocessing && conn->alsos)
    {
	/* reset all of the "beenhere" flags */
	for(sl1 = cfgp->sections.tqh_first; sl1 != NULL; sl1 = sl1->link.tqe_next)
	{
	    sl1->beenhere = FALSE;
	}
	sl->beenhere = TRUE;
	
	/* count them */
	alsos = conn->alsos;
	conn->alsos = NULL;
	for(alsosize=0; alsos[alsosize]!=NULL; alsosize++);

	alsoplace = 0;
	while(alsos != NULL
	      && alsos[alsoplace] != NULL && alsoplace < alsosize
	      && alsoplace < ALSO_LIMIT)
	{
	    /*
	     * for each also= listed, go find this section's keyword list, and
	     * load it as well. This may extend the also= list (and the end),
	     * which we handle by zeroing the also list, and adding to it after
	     * checking for duplicates.
	     */
	    for(sl1 = cfgp->sections.tqh_first;
		sl1 != NULL && strcasecmp(alsos[alsoplace], sl1->name) != 0;
		sl1 = sl1->link.tqe_next);

	    starter_log(LOG_LEVEL_DEBUG, "\twhile loading conn '%s' processing %s"
			, conn->name, alsos[alsoplace]);
			    
	    /*
	     * if we found something that matches by name, and we haven't be there, then
	     * process it.
	     */
	    if(sl1 && !sl1->beenhere)
	    {
		conn->strings_set[KSF_ALSO]=FALSE;
		if(conn->strings[KSF_ALSO]) free(conn->strings[KSF_ALSO]);
		conn->strings[KSF_ALSO]=NULL;
		sl1->beenhere = TRUE;

		/* translate things, but do not replace earlier settings */
		err += translate_conn(conn, sl1, FALSE);

		if(conn->strings[KSF_ALSO])
		{
		    /* now, check out the KSF_ALSO, and extend list if we need to */
		    newalsos = new_list(conn->strings[KSF_ALSO]);		
		    
		    if(newalsos && newalsos[0]!=NULL)
		    {
			/* count them */
			for(newalsoplace=0; newalsos[newalsoplace]!=NULL; newalsoplace++);
			
			/* extend conn->alsos */
			alsos = xrealloc(alsos, (alsosize+newalsoplace) * sizeof(char *));
			for(newalsoplace=0; newalsos[newalsoplace]!=NULL; newalsoplace++)
			{
			    assert(conn != NULL);
			    assert(conn->name != NULL);
			    starter_log(LOG_LEVEL_DEBUG
					, "\twhile processing section '%s' added also=%s"
					, sl1->name, newalsos[newalsoplace]);
			    
			    alsos[alsosize++]=xstrdup(newalsos[newalsoplace]);
			}
		    }
		    
		    free_list(newalsos);
		}
	    }
	    alsoplace++;
	}
	
	if(alsoplace >= ALSO_LIMIT)
	{
	    starter_log(LOG_LEVEL_INFO
			, "while loading conn '%s', too many also= used at section %s. Limit is %d"
			, conn->name
			, conn->alsos[alsoplace]
			, ALSO_LIMIT);
	    return 1;
	}
	
	if(conn->alsos != alsos && conn->alsos != NULL)
	{
	    free_list(conn->alsos);
	}
	conn->alsos = alsos;
    }
    
    

    /* preview of getting rid of transport mode */
    if(conn->options[KBF_TYPE] == KS_TRANSPORT)
    {
	extern struct keyword_enum_values kw_type_list;
	
	starter_log(LOG_LEVEL_INFO, "conn %s - only tunnel mode is supported, not %s"
		    , conn->name
		    , keyword_name(&kw_type_list, conn->options[KBF_TYPE]));
	
	POLICY_ONLY_CONN(conn);
    }
    
    KW_POLICY_FLAG(KBF_TYPE, POLICY_TUNNEL);
    KW_POLICY_FLAG(KBF_COMPRESS, POLICY_COMPRESS);
    KW_POLICY_FLAG(KBF_PFS,  POLICY_PFS);
    
    /* reset authby flags */
    conn->policy &= ~(POLICY_ID_AUTH_MASK);
    conn->policy |= conn->options[KBF_AUTHBY];
    
    KW_POLICY_FLAG(KBF_REKEY, POLICY_DONT_REKEY);
    KW_POLICY_FLAG(KBF_COMPRESS, POLICY_COMPRESS);
    
    err += validate_end(conn, &conn->left,  TRUE, perr);
    err += validate_end(conn, &conn->right, FALSE,perr);

    conn->desired_state = conn->options[KBF_AUTO];
    
    return err;
}

    
static void conn_default (struct starter_conn *conn,
			  struct starter_conn *def)
{
    int i;

    /* structure copy to start */
    *conn = *def;

    /* unlink it */
    memset(&conn->link, 0, sizeof(conn->link));

#define CONN_STR(v) if (v) v=xstrdup(v)
    CONN_STR(conn->left.iface);
    CONN_STR(conn->left.id);
    CONN_STR(conn->left.rsakey1);
    CONN_STR(conn->left.rsakey2);
    CONN_STR(conn->right.iface);
    CONN_STR(conn->right.id);
    CONN_STR(conn->right.rsakey1);
    CONN_STR(conn->right.rsakey2);
    
    for(i=0; i<KSCF_MAX; i++)
    {
	CONN_STR(conn->left.strings[i]);
	CONN_STR(conn->right.strings[i]);
    }
    for(i=0; i<KNCF_MAX; i++)
    {
	conn->left.options[i] = def->left.options[i];
	conn->right.options[i]= def->right.options[i];
    }
    for(i=0 ;i<KSF_MAX; i++)
    {
	CONN_STR(conn->strings[i]);
    }
    for(i=0 ;i<KBF_MAX; i++)
    {
	conn->options[i] = def->options[i];
    }
    
    CONN_STR(conn->esp);
    CONN_STR(conn->ike);
#undef CONN_STR
}

struct starter_config *confread_load(const char *file, char **perr)
{
	struct starter_config *cfg = NULL;
	struct config_parsed *cfgp;
	struct section_list *sconn;
	struct starter_conn *conn;
	unsigned int err = 0;

	/**
	 * Load file
	 */
	cfgp = parser_load_conf(file, perr);
	if (!cfgp) return NULL;

	cfg = (struct starter_config *)malloc(sizeof(struct starter_config));
	if (!cfg) {
		if (perr) *perr = xstrdup("can't allocate mem in confread_load()");
		parser_free_conf(cfgp);
		return NULL;
	}

	/**
	 * Set default values
	 */
	default_values(cfg);

	/**
	 * Load setup
	 */
	err += load_setup(cfg, cfgp, perr);

	if(err) {return NULL;}

	/**
	 * Find %default conn
	 *
	 */
	for(sconn = cfgp->sections.tqh_first; (!err) && sconn != NULL; sconn = sconn->link.tqe_next)
	{
		if (strcmp(sconn->name,"%default")==0) {
			starter_log(LOG_LEVEL_DEBUG, "Loading default conn");
			err += load_conn (cfg, &cfg->conn_default, cfgp, sconn, FALSE, perr);
			if(err == 0)
			{
			    cfg->got_default = TRUE;
			}
		}
	}

	/**
	 * Load other conns
	 */
	for(sconn = cfgp->sections.tqh_first; (!err) && sconn != NULL; sconn = sconn->link.tqe_next)
	{
		if (strcmp(sconn->name,"%default")==0) continue;
		starter_log(LOG_LEVEL_DEBUG, "Loading conn %s", sconn->name);
		conn = (struct starter_conn *)malloc(sizeof(struct starter_conn));
		memset(conn, 0, sizeof(struct starter_conn));
		if (!conn) {
			if (perr) *perr = xstrdup("can't allocate mem in confread_load()");
			parser_free_conf(cfgp);
			confread_free(cfg);
			return NULL;
		}

		if(cfg->got_default)
		{
		    conn_default(conn, &cfg->conn_default);
		}
		conn->name = xstrdup(sconn->name);
		conn->desired_state = STARTUP_NO;
		conn->state = STATE_IGNORE;

		TAILQ_INSERT_TAIL(&cfg->conns, conn, link); 

		err += load_conn (cfg, conn, cfgp, sconn, TRUE, perr);
		
		if(err == 0)
		{
		    conn->state = STATE_LOADED;
		}
	}

	parser_free_conf(cfgp);

	if (err) {
		confread_free(cfg);
		cfg = NULL;
	}

	return cfg;
}

#define FREE_STR(v) { if (v) { free(v); v=NULL; } }
#define FREE_LST(v) { if (v) { free_list(v); v=NULL; } }
static void confread_free_conn(struct starter_conn *conn)
{
    int i;
	FREE_STR(conn->left.iface);
	FREE_STR(conn->left.id);
	FREE_STR(conn->left.rsakey1);
	FREE_STR(conn->left.rsakey2);
	FREE_STR(conn->right.iface);
	FREE_STR(conn->right.id);
	FREE_STR(conn->right.rsakey1);
	FREE_STR(conn->right.rsakey2);
	for(i=0; i<KSCF_MAX; i++)
	{
	    FREE_STR(conn->left.strings[i]);
	    FREE_STR(conn->right.strings[i]);
	}
	for(i=0 ;i<KSF_MAX; i++)
	{
	    FREE_STR(conn->strings[i]);
	}

#ifdef ALG_PATCH
	FREE_STR(conn->esp);
	FREE_STR(conn->ike);
#endif
#ifdef VIRTUAL_IP
	FREE_STR(conn->left.virt);
	FREE_STR(conn->right.virt);
#endif
}

void confread_free(struct starter_config *cfg)
{
    int i;
	struct starter_conn *conn, *c;
	FREE_LST(cfg->setup.interfaces);
#ifdef VIRTUAL_IP
	FREE_STR(cfg->setup.virtual_private);
#endif
	for(i=0 ;i<KSF_MAX; i++)
	{
	    FREE_STR(cfg->setup.strings[i]);
	}
	confread_free_conn(&(cfg->conn_default));

	for(conn = cfg->conns.tqh_first; conn != NULL; )
	{
	    c = conn;
	    conn = conn->link.tqe_next;
	    confread_free_conn(c);
	    free(c);
	}
	free(cfg);
}
#undef FREE_STR

/*
 * Local Variables:
 * c-basic-offset:4
 * c-style: pluto
 * End:
 */
