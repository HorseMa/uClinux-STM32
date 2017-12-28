/*
 * rlm_radutmp.c
 *
 * Version:	$Id: rlm_radutmp.c,v 1.24 2004/02/26 19:04:34 aland Exp $
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Copyright 2000  The FreeRADIUS server project
 * FIXME add copyrights
 */

#include	"autoconf.h"

#include	<sys/types.h>
#include	<stdio.h>
#include	<string.h>
#include	<stdlib.h>
#include	<unistd.h>
#include	<fcntl.h>
#include	<time.h>
#include	<errno.h>
#include        <limits.h>

#include "config.h"

#include	"radiusd.h"
#include	"radutmp.h"
#include	"modules.h"

#define LOCK_LEN sizeof(struct radutmp)

static const char porttypes[] = "ASITX";

/*
 *	used for caching radutmp lookups in the accounting component. The
 *	session (checksimul) component doesn't use it, but probably should.
 */
typedef struct nas_port {
	uint32_t		nasaddr;
	unsigned int	port;
	off_t			offset;
	struct nas_port 	*next;
} NAS_PORT;

typedef struct rlm_radutmp_t {
	NAS_PORT	*nas_port_list;
	char		*filename;
	char		*username;
	int		case_sensitive;
	int		check_nas;
	int		permission;
	int		callerid_ok;
} rlm_radutmp_t;

static CONF_PARSER module_config[] = {
	{ "filename", PW_TYPE_STRING_PTR,
	  offsetof(rlm_radutmp_t,filename), NULL,  RADUTMP },
	{ "username", PW_TYPE_STRING_PTR,
	  offsetof(rlm_radutmp_t,username), NULL,  "%{User-Name}"},
	{ "case_sensitive", PW_TYPE_BOOLEAN,
	  offsetof(rlm_radutmp_t,case_sensitive), NULL,  "yes"},
	{ "check_with_nas", PW_TYPE_BOOLEAN,
	  offsetof(rlm_radutmp_t,check_nas), NULL,  "yes"},
	{ "perm",     PW_TYPE_INTEGER,
	  offsetof(rlm_radutmp_t,permission), NULL,  "0644" },
	{ "callerid", PW_TYPE_BOOLEAN,
	  offsetof(rlm_radutmp_t,callerid_ok), NULL, "no" },
	{ NULL, -1, 0, NULL, NULL }		/* end the list */
};

static int radutmp_instantiate(CONF_SECTION *conf, void **instance)
{
	rlm_radutmp_t *inst;

	inst = rad_malloc(sizeof(*inst));
	if (!inst) {
		return -1;
	}
	memset(inst, 0, sizeof(*inst));

	if (cf_section_parse(conf, inst, module_config)) {
		free(inst);
		return -1;
	}

	inst->nas_port_list = NULL;
	*instance = inst;
	return 0;
}


/*
 *	Detach.
 */
static int radutmp_detach(void *instance)
{
	NAS_PORT *p, *next;
	rlm_radutmp_t *inst = instance;

	for (p = inst->nas_port_list ; p ; p=next) {
		next = p->next;
		free(p);
	}
	if (inst->filename) free(inst->filename);
	if (inst->username) free(inst->username);
	free(inst);
	return 0;
}

/*
 *	Zap all users on a NAS from the radutmp file.
 */
static int radutmp_zap(rlm_radutmp_t *inst,
		       const char *filename,
		       uint32_t nasaddr,
		       time_t t)
{
	struct radutmp	u;
	int		fd;

	if (t == 0) time(&t);

	fd = open(filename, O_RDWR);
	if (fd < 0) {
		radlog(L_ERR, "rlm_radutmp: Error accessing file %s: %s",
		       filename, strerror(errno));
		return RLM_MODULE_FAIL;
	}

	/*
	 *	Lock the utmp file, prefer lockf() over flock().
	 */
	rad_lockfd(fd, LOCK_LEN);

	/*
	 *	Find the entry for this NAS / portno combination.
	 */
	while (read(fd, &u, sizeof(u)) == sizeof(u)) {
	  if ((nasaddr != 0 && nasaddr != u.nas_address) ||
	      u.type != P_LOGIN)
	    continue;
	  /*
	   *	Match. Zap it.
	   */
	  if (lseek(fd, -(off_t)sizeof(u), SEEK_CUR) < 0) {
	    radlog(L_ERR, "rlm_radutmp: radutmp_zap: negative lseek!");
	    lseek(fd, (off_t)0, SEEK_SET);
	  }
	  u.type = P_IDLE;
	  u.time = t;
	  write(fd, &u, sizeof(u));
	}
	close(fd);	/* and implicitely release the locks */

	return 0;
}

/*
 *	Lookup a NAS_PORT in the nas_port_list
 */
static NAS_PORT *nas_port_find(NAS_PORT *nas_port_list, uint32_t nasaddr, unsigned int port)
{
	NAS_PORT	*cl;

	for(cl = nas_port_list; cl; cl = cl->next)
		if (nasaddr == cl->nasaddr &&
			port == cl->port)
			break;
	return cl;
}


/*
 *	Store logins in the RADIUS utmp file.
 */
static int radutmp_accounting(void *instance, REQUEST *request)
{
	struct radutmp	ut, u;
	VALUE_PAIR	*vp;
	int		status = -1;
	uint32_t	nas_address = 0;
	uint32_t	framed_address = 0;
	int		protocol = -1;
	time_t		t;
	int		fd;
	int		just_an_update = 0;
	int		port_seen = 0;
	int		nas_port_type = 0;
	int		off;
	rlm_radutmp_t	*inst = instance;
	char		buffer[256];
	char		filename[1024];
	char		ip_name[32]; /* 255.255.255.255 */
	const char	*nas;
	NAS_PORT	*cache;
	int		r;

	/*
	 *	Which type is this.
	 */
	if ((vp = pairfind(request->packet->vps, PW_ACCT_STATUS_TYPE)) == NULL) {
		radlog(L_ERR, "rlm_radutmp: No Accounting-Status-Type record.");
		return RLM_MODULE_NOOP;
	}
	status = vp->lvalue;

	/*
	 *	Look for weird reboot packets.
	 *
	 *	ComOS (up to and including 3.5.1b20) does not send
	 *	standard PW_STATUS_ACCOUNTING_XXX messages.
	 *
	 *	Check for:  o no Acct-Session-Time, or time of 0
	 *		    o Acct-Session-Id of "00000000".
	 *
	 *	We could also check for NAS-Port, that attribute
	 *	should NOT be present (but we don't right now).
	 */
	if ((status != PW_STATUS_ACCOUNTING_ON) &&
	    (status != PW_STATUS_ACCOUNTING_OFF)) do {
		int check1 = 0;
		int check2 = 0;

		if ((vp = pairfind(request->packet->vps, PW_ACCT_SESSION_TIME))
		     == NULL || vp->lvalue == 0)
			check1 = 1;
		if ((vp = pairfind(request->packet->vps, PW_ACCT_SESSION_ID))
		     != NULL && vp->length == 8 &&
		     memcmp(vp->strvalue, "00000000", 8) == 0)
			check2 = 1;
		if (check1 == 0 || check2 == 0) {
#if 0 /* Cisco sometimes sends START records without username. */
			radlog(L_ERR, "rlm_radutmp: no username in record");
			return RLM_MODULE_FAIL;
#else
			break;
#endif
		}
		radlog(L_INFO, "rlm_radutmp: converting reboot records.");
		if (status == PW_STATUS_STOP)
			status = PW_STATUS_ACCOUNTING_OFF;
		if (status == PW_STATUS_START)
			status = PW_STATUS_ACCOUNTING_ON;
	} while(0);

	time(&t);
	memset(&ut, 0, sizeof(ut));
	ut.porttype = 'A';

	/*
	 *	First, find the interesting attributes.
	 */
	for (vp = request->packet->vps; vp; vp = vp->next) {
		switch (vp->attribute) {
			case PW_LOGIN_IP_HOST:
			case PW_FRAMED_IP_ADDRESS:
				framed_address = vp->lvalue;
				ut.framed_address = vp->lvalue;
				break;
			case PW_FRAMED_PROTOCOL:
				protocol = vp->lvalue;
				break;
			case PW_NAS_IP_ADDRESS:
				nas_address = vp->lvalue;
				ut.nas_address = vp->lvalue;
				break;
			case PW_NAS_PORT:
				ut.nas_port = vp->lvalue;
				port_seen = 1;
				break;
			case PW_ACCT_DELAY_TIME:
				ut.delay = vp->lvalue;
				break;
			case PW_ACCT_SESSION_ID:
				/*
				 *	If length > 8, only store the
				 *	last 8 bytes.
				 */
				off = vp->length - sizeof(ut.session_id);
				/*
				 * 	Ascend is br0ken - it adds a \0
				 * 	to the end of any string.
				 * 	Compensate.
				 */
				if (vp->length > 0 &&
				    vp->strvalue[vp->length - 1] == 0)
					off--;
				if (off < 0) off = 0;
				memcpy(ut.session_id, vp->strvalue + off,
					sizeof(ut.session_id));
				break;
			case PW_NAS_PORT_TYPE:
				if (vp->lvalue <= 4)
					ut.porttype = porttypes[vp->lvalue];
				nas_port_type = vp->lvalue;
				break;
			case PW_CALLING_STATION_ID:
				if(inst->callerid_ok)
					strNcpy(ut.caller_id,
						(char *)vp->strvalue,
						sizeof(ut.caller_id));
				break;
		}
	}

	/*
	 *	If we didn't find out the NAS address, use the
	 *	originator's IP address.
	 */
	if (nas_address == 0) {
		nas_address = request->packet->src_ipaddr;
		ut.nas_address = nas_address;
		nas = client_name(nas_address);	/* MUST be a valid client */

	} else {		/* might be a client, might not be. */
		RADCLIENT *cl;

		/*
		 *	Hack like 'client_name()', but with sane
		 *	fall-back.
		 */
		cl = client_find(nas_address);
		if (cl) {
			if (cl->shortname[0]) {
				nas = cl->shortname;
			} else {
				nas = cl->longname;
			}
		} else {
			/*
			 *	The NAS isn't a client, it's behind
			 *	a proxy server.  In that case, just
			 *	get the IP address.
			 */
			nas = ip_ntoa(ip_name, nas_address);
		}
	}

	/*
	 *	Set the protocol field.
	 */
	if (protocol == PW_PPP)
		ut.proto = 'P';
	else if (protocol == PW_SLIP)
		ut.proto = 'S';
	else
		ut.proto = 'T';
	ut.time = t - ut.delay;

	/*
	 *	Get the utmp filename, via xlat.
	 */
	radius_xlat(filename, sizeof(filename), inst->filename, request, NULL);

	/*
	 *	See if this was a reboot.
	 *
	 *	Hmm... we may not want to zap all of the users when
	 *	the NAS comes up, because of issues with receiving
	 *	UDP packets out of order.
	 */
	if (status == PW_STATUS_ACCOUNTING_ON && nas_address) {
		radlog(L_INFO, "rlm_radutmp: NAS %s restarted (Accounting-On packet seen)",
		       nas);
		radutmp_zap(inst, filename, nas_address, ut.time);
		return RLM_MODULE_OK;
	}

	if (status == PW_STATUS_ACCOUNTING_OFF && nas_address) {
		radlog(L_INFO, "rlm_radutmp: NAS %s rebooted (Accounting-Off packet seen)",
		       nas);
		radutmp_zap(inst, filename, nas_address, ut.time);
		return RLM_MODULE_OK;
	}

	/*
	 *	If we don't know this type of entry pretend we succeeded.
	 */
	if (status != PW_STATUS_START &&
	    status != PW_STATUS_STOP &&
	    status != PW_STATUS_ALIVE) {
		radlog(L_ERR, "rlm_radutmp: NAS %s port %u unknown packet type %d)",
		       nas, ut.nas_port, status);
		return RLM_MODULE_NOOP;
	}

	/*
	 *	Translate the User-Name attribute, or whatever else
	 *	they told us to use.
	 */
	*buffer = '\0';
	radius_xlat(buffer, sizeof(buffer), inst->username, request, NULL);

	/*
	 *  Copy the previous translated user name.
	 */
	strncpy(ut.login, buffer, RUT_NAMESIZE);

	/*
	 *	Perhaps we don't want to store this record into
	 *	radutmp. We skip records:
	 *
	 *	- without a NAS-Port (telnet / tcp access)
	 *	- with the username "!root" (console admin login)
	 */
	if (!port_seen) {
		DEBUG2("  rlm_radutmp: No NAS-Port seen.  Cannot do anything.");
		DEBUG2("  rlm_radumtp: WARNING: checkrad will probably not work!");
		return RLM_MODULE_NOOP;
	}

	if (strncmp(ut.login, "!root", RUT_NAMESIZE) == 0) {
		DEBUG2("  rlm_radutmp: Not recording administrative user");

		return RLM_MODULE_NOOP;
	}

	/*
	 *	Enter into the radutmp file.
	 */
	fd = open(filename, O_RDWR|O_CREAT, inst->permission);
	if (fd < 0) {
		radlog(L_ERR, "rlm_radutmp: Error accessing file %s: %s",
		       filename, strerror(errno));
		return RLM_MODULE_FAIL;
	}

	/*
	 *	Lock the utmp file, prefer lockf() over flock().
	 */
	rad_lockfd(fd, LOCK_LEN);

	/*
	 *	Find the entry for this NAS / portno combination.
	 */
	if ((cache = nas_port_find(inst->nas_port_list, ut.nas_address,
				   ut.nas_port)) != NULL) {
		lseek(fd, (off_t)cache->offset, SEEK_SET);
	}

	r = 0;
	off = 0;
	while (read(fd, &u, sizeof(u)) == sizeof(u)) {
		off += sizeof(u);
		if (u.nas_address != ut.nas_address ||
		    u.nas_port	  != ut.nas_port)
			continue;

		/*
		 *	Don't compare stop records to unused entries.
		 */
		if (status == PW_STATUS_STOP &&
		    u.type == P_IDLE) {
			continue;
		}

		if (status == PW_STATUS_STOP &&
		    strncmp(ut.session_id, u.session_id,
			    sizeof(u.session_id)) != 0) {
			/*
			 *	Don't complain if this is not a
			 *	login record (some clients can
			 *	send _only_ logout records).
			 */
			if (u.type == P_LOGIN)
				radlog(L_ERR, "rlm_radutmp: Logout entry for NAS %s port %u has wrong ID",
				       nas, u.nas_port);
			r = -1;
			break;
		}

		if (status == PW_STATUS_START &&
		    strncmp(ut.session_id, u.session_id,
			    sizeof(u.session_id)) == 0  &&
		    u.time >= ut.time) {
			if (u.type == P_LOGIN) {
				radlog(L_INFO, "rlm_radutmp: Login entry for NAS %s port %u duplicate",
				       nas, u.nas_port);
				r = -1;
				break;
			}
			radlog(L_ERR, "rlm_radutmp: Login entry for NAS %s port %u wrong order",
			       nas, u.nas_port);
			r = -1;
			break;
		}

		/*
		 *	FIXME: the ALIVE record could need
		 *	some more checking, but anyway I'd
		 *	rather rewrite this mess -- miquels.
		 */
		if (status == PW_STATUS_ALIVE &&
		    strncmp(ut.session_id, u.session_id,
			    sizeof(u.session_id)) == 0  &&
		    u.type == P_LOGIN) {
			/*
			 *	Keep the original login time.
			 */
			ut.time = u.time;
			if (u.login[0] != 0)
				just_an_update = 1;
		}

		if (lseek(fd, -(off_t)sizeof(u), SEEK_CUR) < 0) {
			radlog(L_ERR, "rlm_radutmp: negative lseek!");
			lseek(fd, (off_t)0, SEEK_SET);
			off = 0;
		} else
			off -= sizeof(u);
		r = 1;
		break;
	} /* read the file until we find a match */

	/*
	 *	Found the entry, do start/update it with
	 *	the information from the packet.
	 */
	if (r >= 0 &&  (status == PW_STATUS_START ||
			status == PW_STATUS_ALIVE)) {
		/*
		 *	Remember where the entry was, because it's
		 *	easier than searching through the entire file.
		 */
		if (cache == NULL) {
			cache = rad_malloc(sizeof(NAS_PORT));
			cache->nasaddr = ut.nas_address;
			cache->port = ut.nas_port;
			cache->offset = off;
			cache->next = inst->nas_port_list;
			inst->nas_port_list = cache;
		}

		ut.type = P_LOGIN;
		write(fd, &ut, sizeof(u));
	}

	/*
	 *	The user has logged off, delete the entry by
	 *	re-writing it in place.
	 */
	if (status == PW_STATUS_STOP) {
		if (r > 0) {
			u.type = P_IDLE;
			u.time = ut.time;
			u.delay = ut.delay;
			write(fd, &u, sizeof(u));
		} else if (r == 0) {
			radlog(L_ERR, "rlm_radutmp: Logout for NAS %s port %u, but no Login record",
			       nas, ut.nas_port);
			r = -1;
		}
	}
	close(fd);	/* and implicitely release the locks */

	return RLM_MODULE_OK;
}

/*
 *	See if a user is already logged in. Sets request->simul_count to the
 *	current session count for this user and sets request->simul_mpp to 2
 *	if it looks like a multilink attempt based on the requested IP
 *	address, otherwise leaves request->simul_mpp alone.
 *
 *	Check twice. If on the first pass the user exceeds his
 *	max. number of logins, do a second pass and validate all
 *	logins by querying the terminal server (using eg. SNMP).
 */
static int radutmp_checksimul(void *instance, REQUEST *request)
{
	struct radutmp	u;
	int		fd;
	VALUE_PAIR	*vp;
	uint32_t	ipno = 0;
	char		*call_num = NULL;
	int		rcode;
	rlm_radutmp_t	*inst = instance;
	char		login[256];
	char		filename[1024];

	/*
	 *	Get the filename, via xlat.
	 */
	radius_xlat(filename, sizeof(filename), inst->filename, request, NULL);

	if ((fd = open(filename, O_RDWR)) < 0) {
		/*
		 *	If the file doesn't exist, then no users
		 *	are logged in.
		 */
		if (errno == ENOENT) {
			request->simul_count=0;
			return RLM_MODULE_OK;
		}

		/*
		 *	Error accessing the file.
		 */
		radlog(L_ERR, "rlm_radumtp: Error accessing file %s: %s",
		       filename, strerror(errno));
		return RLM_MODULE_FAIL;
	}

	*login = '\0';
	radius_xlat(login, sizeof(login), inst->username, request, NULL);
	if (!*login) {
		return RLM_MODULE_NOOP;
	}

	/*
	 *	WTF?  This is probably wrong... we probably want to
	 *	be able to check users across multiple session accounting
	 *	methods.
	 */
	request->simul_count = 0;

	/*
	 *	Loop over utmp, counting how many people MAY be logged in.
	 */
	while (read(fd, &u, sizeof(u)) == sizeof(u)) {
		if (((strncmp(login, u.login, RUT_NAMESIZE) == 0) ||
		     (!inst->case_sensitive &&
		      (strncasecmp(login, u.login, RUT_NAMESIZE) == 0))) &&
		    (u.type == P_LOGIN)) {
			++request->simul_count;
		}
	}

	/*
	 *	The number of users logged in is OK,
	 *	OR, we've been told to not check the NAS.
	 */
	if ((request->simul_count < request->simul_max) ||
	    !inst->check_nas) {
		close(fd);
		return RLM_MODULE_OK;
	}
	lseek(fd, (off_t)0, SEEK_SET);

	/*
	 *	Setup some stuff, like for MPP detection.
	 */
	if ((vp = pairfind(request->packet->vps, PW_FRAMED_IP_ADDRESS)) != NULL)
		ipno = vp->lvalue;
	if ((vp = pairfind(request->packet->vps, PW_CALLING_STATION_ID)) != NULL)
		call_num = vp->strvalue;

	/*
	 *	lock the file while reading/writing.
	 */
	rad_lockfd(fd, LOCK_LEN);

	/*
	 *	FIXME: If we get a 'Start' for a user/nas/port which is
	 *	listed, but for which we did NOT get a 'Stop', then
	 *	it's not a duplicate session.  This happens with
	 *	static IP's like DSL.
	 */
	request->simul_count = 0;
	while (read(fd, &u, sizeof(u)) == sizeof(u)) {
		if (((strncmp(login, u.login, RUT_NAMESIZE) == 0) ||
		     (!inst->case_sensitive &&
		      (strncasecmp(login, u.login, RUT_NAMESIZE) == 0))) &&
		    (u.type == P_LOGIN)) {
			char session_id[sizeof(u.session_id) + 1];
			char utmp_login[sizeof(u.login) + 1];

			strNcpy(session_id, u.session_id, sizeof(session_id));

			/*
			 *	The login name MAY fill the whole field,
			 *	and thus won't be zero-filled.
			 *
			 *	Note that we take the user name from
			 *	the utmp file, as that's the canonical
			 *	form.  The 'login' variable may contain
			 *	a string which is an upper/lowercase
			 *	version of u.login.  When we call the
			 *	routine to check the terminal server,
			 *	the NAS may be case sensitive.
			 *
			 *	e.g. We ask if "bob" is using a port,
			 *	and the NAS says "no", because "BOB"
			 *	is using the port.
			 */
			strNcpy(utmp_login, u.login, sizeof(u.login));

			/*
			 *	rad_check_ts may take seconds
			 *	to return, and we don't want
			 *	to block everyone else while
			 *	that's happening.  */
			rad_unlockfd(fd, LOCK_LEN);
			rcode = rad_check_ts(u.nas_address, u.nas_port,
					     utmp_login, session_id);
			rad_lockfd(fd, LOCK_LEN);

			/*
			 *	Failed to check the terminal server for
			 *	duplicate logins: Return an error.
			 */
			if (rcode < 0) {
				close(fd);
				return RLM_MODULE_FAIL;
			}

			if (rcode == 1) {
				++request->simul_count;

				/*
				 *	Does it look like a MPP attempt?
				 */
				if (strchr("SCPA", u.proto) &&
				    ipno && u.framed_address == ipno)
					request->simul_mpp = 2;
				else if (strchr("SCPA", u.proto) && call_num &&
					!strncmp(u.caller_id,call_num,16))
					request->simul_mpp = 2;
			}
			else {
				/*
				 *	False record - zap it.
				 */
				session_zap(request,
					    u.nas_address, u.nas_port, login,
					    session_id, u.framed_address,
					    u.proto);
			}
		}
	}
	close(fd);		/* and implicitely release the locks */

	return RLM_MODULE_OK;
}

/* globally exported name */
module_t rlm_radutmp = {
  "radutmp",
  0,                            /* type: reserved */
  NULL,                 	/* initialization */
  radutmp_instantiate,          /* instantiation */
  {
	  NULL,                 /* authentication */
	  NULL,                 /* authorization */
	  NULL,                 /* preaccounting */
	  radutmp_accounting,   /* accounting */
	  radutmp_checksimul,	/* checksimul */
	  NULL,			/* pre-proxy */
	  NULL,			/* post-proxy */
	  NULL			/* post-auth */
  },
  radutmp_detach,               /* detach */
  NULL,         	        /* destroy */
};

