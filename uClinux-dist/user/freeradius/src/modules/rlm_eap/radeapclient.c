/*
 * radeapclient.c	EAP specific radius packet debug tool.
 *
 * Version:	$Id: radeapclient.c,v 1.7.4.5 2006/05/19 14:22:23 nbk Exp $
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
 * Copyright 2000  Miquel van Smoorenburg <miquels@cistron.nl>
 * Copyright 2000  Alan DeKok <aland@ox.org>
 */
static const char rcsid[] = "$Id: radeapclient.c,v 1.7.4.5 2006/05/19 14:22:23 nbk Exp $";

#include "autoconf.h"

#include <stdio.h>
#include <stdlib.h>

#if HAVE_UNISTD_H
#	include <unistd.h>
#endif

#include <string.h>
#include <ctype.h>
#include <netdb.h>
#include <sys/socket.h>

#if HAVE_NETINET_IN_H
#	include <netinet/in.h>
#endif

#if HAVE_SYS_SELECT_H
#	include <sys/select.h>
#endif

#if HAVE_GETOPT_H
#	include <getopt.h>
#endif

#include "conf.h"
#include "radpaths.h"
#include "missing.h"
#include "../../include/md5.h"

#include "eap_types.h"
#include "eap_sim.h"

extern int sha1_data_problems;

static int retries = 10;
static float timeout = 3;
static const char *secret = NULL;
static int do_output = 1;
static int do_summary = 0;
static int filedone = 0;
static int totalapp = 0;
static int totaldeny = 0;
static char filesecret[256];
const char *radius_dir = RADDBDIR;
const char *progname = "radeapclient";
/* lrad_randctx randctx; */


radlog_dest_t radlog_dest = RADLOG_STDERR;
const char *radlog_dir = NULL;
int debug_flag = 0;
struct main_config_t mainconfig;
char password[256];

struct eapsim_keys eapsim_mk;

static void NEVER_RETURNS usage(void)
{
	fprintf(stderr, "Usage: radeapclient [options] server[:port] <command> [<secret>]\n");

	fprintf(stderr, "  <command>    One of auth, acct, status, or disconnect.\n");
	fprintf(stderr, "  -c count    Send each packet 'count' times.\n");
	fprintf(stderr, "  -d raddb    Set dictionary directory.\n");
	fprintf(stderr, "  -f file     Read packets from file, not stdin.\n");
	fprintf(stderr, "  -r retries  If timeout, retry sending the packet 'retries' times.\n");
	fprintf(stderr, "  -t timeout  Wait 'timeout' seconds before retrying (may be a floating point number).\n");
	fprintf(stderr, "  -i id       Set request id to 'id'.  Values may be 0..255\n");
	fprintf(stderr, "  -S file     read secret from file, not command line.\n");
	fprintf(stderr, "  -q          Do not print anything out.\n");
	fprintf(stderr, "  -s          Print out summary information of auth results.\n");
	fprintf(stderr, "  -v          Show program version information.\n");
	fprintf(stderr, "  -x          Debugging mode.\n");

	exit(1);
}

int radlog(int lvl, const char *msg, ...)
{
	va_list ap;
	int r;

	r = lvl; /* shut up compiler */

	va_start(ap, msg);
	r = vfprintf(stderr, msg, ap);
	va_end(ap);
	fputc('\n', stderr);

	return r;
}

int log_debug(const char *msg, ...)
{
	va_list ap;
	int r;

	va_start(ap, msg);
	r = vfprintf(stderr, msg, ap);
	va_end(ap);
	fputc('\n', stderr);

	return r;
}

static int getport(const char *name)
{
	struct	servent		*svp;

	svp = getservbyname (name, "udp");
	if (!svp) {
		return 0;
	}

	return ntohs(svp->s_port);
}

static int send_packet(RADIUS_PACKET *req, RADIUS_PACKET **rep)
{
	int i;
	struct timeval	tv;

	for (i = 0; i < retries; i++) {
		fd_set		rdfdesc;

		rad_send(req, NULL, secret);

		/* And wait for reply, timing out as necessary */
		FD_ZERO(&rdfdesc);
		FD_SET(req->sockfd, &rdfdesc);

		tv.tv_sec = (int)timeout;
		tv.tv_usec = 1000000 * (timeout - (int) timeout);

		/* Something's wrong if we don't get exactly one fd. */
		if (select(req->sockfd + 1, &rdfdesc, NULL, NULL, &tv) != 1) {
			continue;
		}

		*rep = rad_recv(req->sockfd);
		if (*rep != NULL) {
			/*
			 *	If we get a response from a machine
			 *	which we did NOT send a request to,
			 *	then complain.
			 */
			if (((*rep)->src_ipaddr != req->dst_ipaddr) ||
			    ((*rep)->src_port != req->dst_port)) {
				char src[64], dst[64];

				ip_ntoa(src, (*rep)->src_ipaddr);
				ip_ntoa(dst, req->dst_ipaddr);
				fprintf(stderr, "radclient: ERROR: Sent request to host %s port %d, got response from host %s port %d\n!",
					dst, req->dst_port,
					src, (*rep)->src_port);
				exit(1);
			}
			break;
		} else {	/* NULL: couldn't receive the packet */
			librad_perror("radclient:");
			exit(1);
		}
	}

	/* No response or no data read (?) */
	if (i == retries) {
		fprintf(stderr, "radclient: no response from server\n");
		exit(1);
	}

	/*
	 *	FIXME: Discard the packet & listen for another.
	 *
	 *	Hmm... we should really be using eapol_test, which does
	 *	a lot more than radeapclient.
	 */
	if (rad_verify(*rep, req, secret) != 0) {
		librad_perror("rad_verify");
		exit(1);
	}

	if (rad_decode(*rep, req, secret) != 0) {
		librad_perror("rad_decode");
		exit(1);
	}

	/* libradius debug already prints out the value pairs for us */
	if (!librad_debug && do_output) {
		printf("Received response ID %d, code %d, length = %d\n",
				(*rep)->id, (*rep)->code, (*rep)->data_len);
		vp_printlist(stdout, (*rep)->vps);
	}
	if((*rep)->code == PW_AUTHENTICATION_ACK) {
		totalapp++;
	} else {
		totaldeny++;
	}

	return 0;
}

static void cleanresp(RADIUS_PACKET *resp)
{
	VALUE_PAIR *vpnext, *vp, **last;


	/*
	 * maybe should just copy things we care about, or keep
	 * a copy of the original input and start from there again?
	 */
	pairdelete(&resp->vps, PW_EAP_MESSAGE);
	pairdelete(&resp->vps, ATTRIBUTE_EAP_BASE+PW_EAP_IDENTITY);

	last = &resp->vps;
	for(vp = *last; vp != NULL; vp = vpnext)
	{
		vpnext = vp->next;

		if((vp->attribute > ATTRIBUTE_EAP_BASE &&
		    vp->attribute <= ATTRIBUTE_EAP_BASE+256) ||
		   (vp->attribute > ATTRIBUTE_EAP_SIM_BASE &&
		    vp->attribute <= ATTRIBUTE_EAP_SIM_BASE+256))
		{
			*last = vpnext;
			pairbasicfree(vp);
		} else {
			last = &vp->next;
		}
	}
}

/*
 * we got an EAP-Request/Sim/Start message in a legal state.
 *
 * pick a supported version, put it into the reply, and insert a nonce.
 */
static int process_eap_start(RADIUS_PACKET *req,
			     RADIUS_PACKET *rep)
{
	VALUE_PAIR *vp, *newvp;
	VALUE_PAIR *anyidreq_vp, *fullauthidreq_vp, *permanentidreq_vp;
	uint16_t *versions, selectedversion;
	unsigned int i,versioncount;

	/* form new response clear of any EAP stuff */
	cleanresp(rep);

	if((vp = pairfind(req->vps, ATTRIBUTE_EAP_SIM_BASE+PW_EAP_SIM_VERSION_LIST)) == NULL) {
		fprintf(stderr, "illegal start message has no VERSION_LIST\n");
		return 0;
	}

	versions = (uint16_t *)vp->strvalue;

	/* verify that the attribute length is big enough for a length field */
	if(vp->length < 4)
	{
		fprintf(stderr, "start message has illegal VERSION_LIST. Too short: %d\n", vp->length);
		return 0;
	}

	versioncount = ntohs(versions[0])/2;
	/* verify that the attribute length is big enough for the given number
	 * of versions present.
	 */
	if((unsigned)vp->length <= (versioncount*2 + 2))
	{
		fprintf(stderr, "start message is too short. Claimed %d versions does not fit in %d bytes\n", versioncount, vp->length);
		return 0;
	}

	/*
	 * record the versionlist for the MK calculation.
	 */
	eapsim_mk.versionlistlen = versioncount*2;
	memcpy(eapsim_mk.versionlist, (unsigned char *)(versions+1),
	       eapsim_mk.versionlistlen);

	/* walk the version list, and pick the one we support, which
	 * at present, is 1, EAP_SIM_VERSION.
	 */
	selectedversion=0;
	for(i=0; i < versioncount; i++)
	{
		if(ntohs(versions[i+1]) == EAP_SIM_VERSION)
		{
			selectedversion=EAP_SIM_VERSION;
			break;
		}
	}
	if(selectedversion == 0)
	{
		fprintf(stderr, "eap-sim start message. No compatible version found. We need %d\n", EAP_SIM_VERSION);
		for(i=0; i < versioncount; i++)
		{
			fprintf(stderr, "\tfound version %d\n",
				ntohs(versions[i+1]));
		}
	}

	/*
	 * now make sure that we have only FULLAUTH_ID_REQ.
	 * I think that it actually might not matter - we can answer in
	 * anyway we like, but it is illegal to have more than one
	 * present.
	 */
	anyidreq_vp = pairfind(req->vps, ATTRIBUTE_EAP_SIM_BASE+PW_EAP_SIM_ANY_ID_REQ);
	fullauthidreq_vp = pairfind(req->vps, ATTRIBUTE_EAP_SIM_BASE+PW_EAP_SIM_FULLAUTH_ID_REQ);
	permanentidreq_vp = pairfind(req->vps, ATTRIBUTE_EAP_SIM_BASE+PW_EAP_SIM_PERMANENT_ID_REQ);

	if(fullauthidreq_vp == NULL ||
	   anyidreq_vp != NULL ||
	   permanentidreq_vp != NULL) {
		fprintf(stderr, "start message has %sanyidreq, %sfullauthid and %spermanentid. Illegal combination.\n",
			(anyidreq_vp != NULL ? "a " : "no "),
			(fullauthidreq_vp != NULL ? "a " : "no "),
			(permanentidreq_vp != NULL ? "a " : "no "));
		return 0;
	}

	/* okay, we have just any_id_req there, so fill in response */

	/* mark the subtype as being EAP-SIM/Response/Start */
	newvp = paircreate(ATTRIBUTE_EAP_SIM_SUBTYPE, PW_TYPE_INTEGER);
	newvp->lvalue = eapsim_start;
	pairreplace(&(rep->vps), newvp);

	/* insert selected version into response. */
	newvp = paircreate(ATTRIBUTE_EAP_SIM_BASE+PW_EAP_SIM_SELECTED_VERSION,
			   PW_TYPE_OCTETS);
	versions = (uint16_t *)newvp->strvalue;
	versions[0] = htons(selectedversion);
	newvp->length = 2;
	pairreplace(&(rep->vps), newvp);

	/* record the selected version */
	memcpy(eapsim_mk.versionselect, (unsigned char *)versions, 2);

	vp = newvp = NULL;

	{
		uint32_t nonce[4];
		/*
		 * insert a nonce_mt that we make up.
		 */
		newvp = paircreate(ATTRIBUTE_EAP_SIM_BASE+PW_EAP_SIM_NONCE_MT,
				   PW_TYPE_OCTETS);
		newvp->strvalue[0]=0;
		newvp->strvalue[1]=0;
		newvp->length = 18;  /* 16 bytes of nonce + padding */

		nonce[0]=lrad_rand();
		nonce[1]=lrad_rand();
		nonce[2]=lrad_rand();
		nonce[3]=lrad_rand();
		memcpy(&newvp->strvalue[2], nonce, 16);
		pairreplace(&(rep->vps), newvp);

		/* also keep a copy of the nonce! */
		memcpy(eapsim_mk.nonce_mt, nonce, 16);
	}

	{
		uint16_t *pidlen, idlen;

		/*
		 * insert the identity here.
		 */
		vp = pairfind(rep->vps, PW_USER_NAME);
		if(vp == NULL)
		{
			fprintf(stderr, "eap-sim: We need to have a User-Name attribute!\n");
			return 0;
		}
		newvp = paircreate(ATTRIBUTE_EAP_SIM_BASE+PW_EAP_SIM_IDENTITY,
				   PW_TYPE_OCTETS);
		idlen = strlen(vp->strvalue);
		pidlen = (uint16_t *)newvp->strvalue;
		*pidlen = htons(idlen);
		newvp->length = idlen + 2;

		memcpy(&newvp->strvalue[2], vp->strvalue, idlen);
		pairreplace(&(rep->vps), newvp);

		/* record it */
		memcpy(eapsim_mk.identity, vp->strvalue, idlen);
		eapsim_mk.identitylen = idlen;
	}

	return 1;
}

/*
 * we got an EAP-Request/Sim/Challenge message in a legal state.
 *
 * use the RAND challenge to produce the SRES result, and then
 * use that to generate a new MAC.
 *
 * for the moment, we ignore the RANDs, then just plug in the SRES
 * values.
 *
 */
static int process_eap_challenge(RADIUS_PACKET *req,
				 RADIUS_PACKET *rep)
{
	VALUE_PAIR *newvp;
	VALUE_PAIR *mac, *randvp;
	VALUE_PAIR *sres1,*sres2,*sres3;
	VALUE_PAIR *Kc1, *Kc2, *Kc3;
	uint8_t calcmac[20];

	/* look for the AT_MAC and the challenge data */
	mac   = pairfind(req->vps, ATTRIBUTE_EAP_SIM_BASE+PW_EAP_SIM_MAC);
	randvp= pairfind(req->vps, ATTRIBUTE_EAP_SIM_BASE+PW_EAP_SIM_RAND);
	if(mac == NULL || rand == NULL) {
		fprintf(stderr, "radeapclient: challenge message needs to contain RAND and MAC\n");
		return 0;
	}

	/*
	 * compare RAND with randX, to verify this is the right response
	 * to this challenge.
	 */
	{
	  VALUE_PAIR *randcfgvp[3];
	  unsigned char *randcfg[3];

	  randcfg[0] = &randvp->strvalue[2];
	  randcfg[1] = &randvp->strvalue[2+EAPSIM_RAND_SIZE];
	  randcfg[2] = &randvp->strvalue[2+EAPSIM_RAND_SIZE*2];

	  randcfgvp[0] = pairfind(rep->vps, ATTRIBUTE_EAP_SIM_RAND1);
	  randcfgvp[1] = pairfind(rep->vps, ATTRIBUTE_EAP_SIM_RAND2);
	  randcfgvp[2] = pairfind(rep->vps, ATTRIBUTE_EAP_SIM_RAND3);

	  if(randcfgvp[0] == NULL ||
	     randcfgvp[1] == NULL ||
	     randcfgvp[2] == NULL) {
	    fprintf(stderr, "radeapclient: needs to have rand1, 2 and 3 set.\n");
	    return 0;
	  }

	  if(memcmp(randcfg[0], randcfgvp[0]->strvalue, EAPSIM_RAND_SIZE)!=0 ||
	     memcmp(randcfg[1], randcfgvp[1]->strvalue, EAPSIM_RAND_SIZE)!=0 ||
	     memcmp(randcfg[2], randcfgvp[2]->strvalue, EAPSIM_RAND_SIZE)!=0) {
	    int rnum,i,j;

	    fprintf(stderr, "radeapclient: one of rand 1,2,3 didn't match\n");
	    for(rnum = 0; rnum < 3; rnum++) {
	      fprintf(stderr, "received   rand %d: ", rnum);
	      j=0;
	      for (i = 0; i < EAPSIM_RAND_SIZE; i++) {
		if(j==4) {
		  printf("_");
		  j=0;
		}
		j++;

		fprintf(stderr, "%02x", randcfg[rnum][i]);
	      }
	      fprintf(stderr, "\nconfigured rand %d: ", rnum);
	      j=0;
	      for (i = 0; i < EAPSIM_RAND_SIZE; i++) {
		if(j==4) {
		  printf("_");
		  j=0;
		}
		j++;

		fprintf(stderr, "%02x", randcfgvp[rnum]->strvalue[i]);
	      }
	      fprintf(stderr, "\n");
	    }
	    return 0;
	  }
	}

	/*
	 * now dig up the sres values from the response packet,
	 * which were put there when we read things in.
	 *
	 * Really, they should be calculated from the RAND!
	 *
	 */
	sres1 = pairfind(rep->vps, ATTRIBUTE_EAP_SIM_SRES1);
	sres2 = pairfind(rep->vps, ATTRIBUTE_EAP_SIM_SRES2);
	sres3 = pairfind(rep->vps, ATTRIBUTE_EAP_SIM_SRES3);

	if(sres1 == NULL ||
	   sres2 == NULL ||
	   sres3 == NULL) {
		fprintf(stderr, "radeapclient: needs to have sres1, 2 and 3 set.\n");
		return 0;
	}
	memcpy(eapsim_mk.sres[0], sres1->strvalue, sizeof(eapsim_mk.sres[0]));
	memcpy(eapsim_mk.sres[1], sres2->strvalue, sizeof(eapsim_mk.sres[1]));
	memcpy(eapsim_mk.sres[2], sres3->strvalue, sizeof(eapsim_mk.sres[2]));

	Kc1 = pairfind(rep->vps, ATTRIBUTE_EAP_SIM_KC1);
	Kc2 = pairfind(rep->vps, ATTRIBUTE_EAP_SIM_KC2);
	Kc3 = pairfind(rep->vps, ATTRIBUTE_EAP_SIM_KC3);

	if(Kc1 == NULL ||
	   Kc2 == NULL ||
	   Kc3 == NULL) {
		fprintf(stderr, "radeapclient: needs to have Kc1, 2 and 3 set.\n");
		return 0;
	}
	memcpy(eapsim_mk.Kc[0], Kc1->strvalue, sizeof(eapsim_mk.Kc[0]));
	memcpy(eapsim_mk.Kc[1], Kc2->strvalue, sizeof(eapsim_mk.Kc[1]));
	memcpy(eapsim_mk.Kc[2], Kc3->strvalue, sizeof(eapsim_mk.Kc[2]));

	/* all set, calculate keys */
	eapsim_calculate_keys(&eapsim_mk);

	if(debug_flag) {
	  eapsim_dump_mk(&eapsim_mk);
	}

	/* verify the MAC, now that we have all the keys. */
	if(eapsim_checkmac(req->vps, eapsim_mk.K_aut,
			   eapsim_mk.nonce_mt, sizeof(eapsim_mk.nonce_mt),
			   calcmac)) {
		printf("MAC check succeed\n");
	} else {
		int i, j;
		j=0;
		printf("calculated MAC (");
		for (i = 0; i < 20; i++) {
			if(j==4) {
				printf("_");
				j=0;
			}
			j++;

			printf("%02x", calcmac[i]);
		}
		printf(" did not match\n");
		return 0;
	}

	/* form new response clear of any EAP stuff */
	cleanresp(rep);

	/* mark the subtype as being EAP-SIM/Response/Start */
	newvp = paircreate(ATTRIBUTE_EAP_SIM_SUBTYPE, PW_TYPE_INTEGER);
	newvp->lvalue = eapsim_challenge;
	pairreplace(&(rep->vps), newvp);

	/*
	 * fill the SIM_MAC with a field that will in fact get appended
	 * to the packet before the MAC is calculated
	 */
	newvp = paircreate(ATTRIBUTE_EAP_SIM_BASE+PW_EAP_SIM_MAC,
			   PW_TYPE_OCTETS);
	memcpy(newvp->strvalue+EAPSIM_SRES_SIZE*0, sres1->strvalue, EAPSIM_SRES_SIZE);
	memcpy(newvp->strvalue+EAPSIM_SRES_SIZE*1, sres2->strvalue, EAPSIM_SRES_SIZE);
	memcpy(newvp->strvalue+EAPSIM_SRES_SIZE*2, sres3->strvalue, EAPSIM_SRES_SIZE);
	newvp->length = EAPSIM_SRES_SIZE*3;
	pairreplace(&(rep->vps), newvp);

	newvp = paircreate(ATTRIBUTE_EAP_SIM_KEY, PW_TYPE_OCTETS);
	memcpy(newvp->strvalue,    eapsim_mk.K_aut, EAPSIM_AUTH_SIZE);
	newvp->length = EAPSIM_AUTH_SIZE;
	pairreplace(&(rep->vps), newvp);

	return 1;
}

/*
 * this code runs the EAP-SIM client state machine.
 * the *request* is from the server.
 * the *reponse* is to the server.
 *
 */
static int respond_eap_sim(RADIUS_PACKET *req,
			   RADIUS_PACKET *resp)
{
	enum eapsim_clientstates state, newstate;
	enum eapsim_subtype subtype;
	VALUE_PAIR *vp, *statevp, *radstate, *eapid;
	char statenamebuf[32], subtypenamebuf[32];

	if ((radstate = paircopy2(req->vps, PW_STATE)) == NULL)
	{
		return 0;
	}

	if ((eapid = paircopy2(req->vps, ATTRIBUTE_EAP_ID)) == NULL)
	{
		return 0;
	}

	/* first, dig up the state from the request packet, setting
	 * outselves to be in EAP-SIM-Start state if there is none.
	 */

	if((statevp = pairfind(resp->vps, ATTRIBUTE_EAP_SIM_STATE)) == NULL)
	{
		/* must be initial request */
		statevp = paircreate(ATTRIBUTE_EAP_SIM_STATE, PW_TYPE_INTEGER);
		statevp->lvalue = eapsim_client_init;
		pairreplace(&(resp->vps), statevp);
	}
	state = statevp->lvalue;

	/*
	 * map the attributes, and authenticate them.
	 */
	unmap_eapsim_types(req);

	printf("<+++ EAP-sim decoded packet:\n");
	vp_printlist(stdout, req->vps);

	if((vp = pairfind(req->vps, ATTRIBUTE_EAP_SIM_SUBTYPE)) == NULL)
	{
		return 0;
	}
	subtype = vp->lvalue;

	/*
	 * look for the appropriate state, and process incoming message
	 */
	switch(state) {
	case eapsim_client_init:
		switch(subtype) {
		case eapsim_start:
			newstate = process_eap_start(req, resp);
			break;

		case eapsim_challenge:
		case eapsim_notification:
		case eapsim_reauth:
		default:
			fprintf(stderr, "radeapclient: sim in state %s message %s is illegal. Reply dropped.\n",
				sim_state2name(state, statenamebuf, sizeof(statenamebuf)),
				sim_subtype2name(subtype, subtypenamebuf, sizeof(subtypenamebuf)));
			/* invalid state, drop message */
			return 0;
		}
		break;

	case eapsim_client_start:
		switch(subtype) {
		case eapsim_start:
			/* NOT SURE ABOUT THIS ONE, retransmit, I guess */
			newstate = process_eap_start(req, resp);
			break;

		case eapsim_challenge:
			newstate = process_eap_challenge(req, resp);
			break;

		default:
			fprintf(stderr, "radeapclient: sim in state %s message %s is illegal. Reply dropped.\n",
				sim_state2name(state, statenamebuf, sizeof(statenamebuf)),
				sim_subtype2name(subtype, subtypenamebuf, sizeof(subtypenamebuf)));
			/* invalid state, drop message */
			return 0;
		}
		break;


	default:
		fprintf(stderr, "radeapclient: sim in illegal state %s\n",
			sim_state2name(state, statenamebuf, sizeof(statenamebuf)));
		return 0;
	}

	/* copy the eap state object in */
	pairreplace(&(resp->vps), eapid);

	/* update stete info, and send new packet */
	map_eapsim_types(resp);

	/* copy the radius state object in */
	pairreplace(&(resp->vps), radstate);

	statevp->lvalue = newstate;
	return 1;
}

static int respond_eap_md5(RADIUS_PACKET *req,
			   RADIUS_PACKET *rep)
{
	VALUE_PAIR *vp, *id, *state;
	int valuesize, namesize;
	unsigned char identifier;
	unsigned char *value;
	unsigned char *name;
	MD5_CTX	context;
	char    response[16];

	cleanresp(rep);

	if ((state = paircopy2(req->vps, PW_STATE)) == NULL)
	{
		fprintf(stderr, "radeapclient: no state attribute found\n");
		return 0;
	}

	if ((id = paircopy2(req->vps, ATTRIBUTE_EAP_ID)) == NULL)
	{
		fprintf(stderr, "radeapclient: no EAP-ID attribute found\n");
		return 0;
	}
	identifier = id->lvalue;

	if ((vp = pairfind(req->vps, ATTRIBUTE_EAP_BASE+PW_EAP_MD5)) == NULL)
	{
		fprintf(stderr, "radeapclient: no EAP-MD5 attribute found\n");
		return 0;
	}

	/* got the details of the MD5 challenge */
	valuesize = vp->strvalue[0];
	value = &vp->strvalue[1];
	name  = &vp->strvalue[valuesize+1];
	namesize = vp->length - (valuesize + 1);

	/* sanitize items */
	if(valuesize > vp->length)
	{
		fprintf(stderr, "radeapclient: md5 valuesize if too big (%d > %d)\n",
			valuesize, vp->length);
		return 0;
	}

	/* now do the CHAP operation ourself, rather than build the
	 * buffer. We could also call rad_chap_encode, but it wants
	 * a CHAP-Challenge, which we don't want to bother with.
	 */
	librad_MD5Init(&context);
	librad_MD5Update(&context, &identifier, 1);
	librad_MD5Update(&context, password, strlen(password));
	librad_MD5Update(&context, value, valuesize);
	librad_MD5Final(response, &context);

	vp = paircreate(ATTRIBUTE_EAP_BASE+PW_EAP_MD5, PW_TYPE_OCTETS);
	vp->strvalue[0]=16;
	memcpy(&vp->strvalue[1], response, 16);
	vp->length = 17;

	pairreplace(&(rep->vps), vp);

	pairreplace(&(rep->vps), id);

	/* copy the state object in */
	pairreplace(&(rep->vps), state);

	return 1;
}



static int sendrecv_eap(RADIUS_PACKET *rep)
{
	RADIUS_PACKET *req = NULL;
	VALUE_PAIR *vp, *vpnext;
	int tried_eap_md5 = 0;

	/*
	 *	Keep a copy of the the User-Password attribute.
	 */
	if ((vp = pairfind(rep->vps, ATTRIBUTE_EAP_MD5_PASSWORD)) != NULL) {
		strNcpy(password, (char *)vp->strvalue, sizeof(vp->strvalue));

	} else 	if ((vp = pairfind(rep->vps, PW_PASSWORD)) != NULL) {
		strNcpy(password, (char *)vp->strvalue, sizeof(vp->strvalue));
		/*
		 *	Otherwise keep a copy of the CHAP-Password attribute.
		 */
	} else if ((vp = pairfind(rep->vps, PW_CHAP_PASSWORD)) != NULL) {
		strNcpy(password, (char *)vp->strvalue, sizeof(vp->strvalue));
	} else {
		*password = '\0';
	}

 again:
	rep->id++;

	printf("\n+++> About to send encoded packet:\n");
	vp_printlist(stdout, rep->vps);

	/*
	 * if there are EAP types, encode them into an EAP-Message
	 *
	 */
	map_eap_types(rep);

	/*
	 *  Fix up Digest-Attributes issues
	 */
	for (vp = rep->vps; vp != NULL; vp = vp->next) {
		switch (vp->attribute) {
		default:
			break;

		case PW_DIGEST_REALM:
		case PW_DIGEST_NONCE:
		case PW_DIGEST_METHOD:
		case PW_DIGEST_URI:
		case PW_DIGEST_QOP:
		case PW_DIGEST_ALGORITHM:
		case PW_DIGEST_BODY_DIGEST:
		case PW_DIGEST_CNONCE:
		case PW_DIGEST_NONCE_COUNT:
		case PW_DIGEST_USER_NAME:
			/* overlapping! */
			memmove(&vp->strvalue[2], &vp->strvalue[0], vp->length);
			vp->strvalue[0] = vp->attribute - PW_DIGEST_REALM + 1;
			vp->length += 2;
			vp->strvalue[1] = vp->length;
			vp->attribute = PW_DIGEST_ATTRIBUTES;
			break;
		}
	}

	/*
	 *	If we've already sent a packet, free up the old
	 *	one, and ensure that the next packet has a unique
	 *	ID and authentication vector.
	 */
	if (rep->data) {
		free(rep->data);
		rep->data = NULL;
	}

	librad_md5_calc(rep->vector, rep->vector,
			sizeof(rep->vector));

	if (*password != '\0') {
		if ((vp = pairfind(rep->vps, PW_PASSWORD)) != NULL) {
			strNcpy((char *)vp->strvalue, password, strlen(password) + 1);
			vp->length = strlen(password);

		} else if ((vp = pairfind(rep->vps, PW_CHAP_PASSWORD)) != NULL) {
			strNcpy((char *)vp->strvalue, password, strlen(password) + 1);
			vp->length = strlen(password);

			rad_chap_encode(rep, (char *) vp->strvalue, rep->id, vp);
			vp->length = 17;
		}
	} /* there WAS a password */

	/* send the response, wait for the next request */
	send_packet(rep, &req);

	/* okay got back the packet, go and decode the EAP-Message. */
	unmap_eap_types(req);

	printf("<+++ EAP decoded packet:\n");
	vp_printlist(stdout, req->vps);

	/* now look for the code type. */
	for (vp = req->vps; vp != NULL; vp = vpnext) {
		vpnext = vp->next;

		switch (vp->attribute) {
		default:
			break;

		case ATTRIBUTE_EAP_BASE+PW_EAP_MD5:
			if(respond_eap_md5(req, rep) && tried_eap_md5 < 3)
			{
				tried_eap_md5++;
				goto again;
			}
			break;

		case ATTRIBUTE_EAP_BASE+PW_EAP_SIM:
			if(respond_eap_sim(req, rep))
			{
				goto again;
			}
			break;
		}
	}

	return 1;
}


int main(int argc, char **argv)
{
	RADIUS_PACKET *req;
	char *p;
	int c;
	int port = 0;
	char *filename = NULL;
	FILE *fp;
	int count = 1;
	int id;

	id = ((int)getpid() & 0xff);
	librad_debug = 0;

	radlog_dest = RADLOG_STDERR;

	while ((c = getopt(argc, argv, "c:d:f:hi:qst:r:S:xXv")) != EOF)
	{
		switch(c) {
		case 'c':
			if (!isdigit((int) *optarg))
				usage();
			count = atoi(optarg);
			break;
		case 'd':
			radius_dir = optarg;
			break;
		case 'f':
			filename = optarg;
			break;
		case 'q':
			do_output = 0;
			break;
		case 'x':
		        debug_flag++;
			librad_debug++;
			break;

		case 'X':
#if 0
		  sha1_data_problems = 1; /* for debugging only */
#endif
		  break;



		case 'r':
			if (!isdigit((int) *optarg))
				usage();
			retries = atoi(optarg);
			break;
		case 'i':
			if (!isdigit((int) *optarg))
				usage();
			id = atoi(optarg);
			if ((id < 0) || (id > 255)) {
				usage();
			}
			break;
		case 's':
			do_summary = 1;
			break;
		case 't':
			if (!isdigit((int) *optarg))
				usage();
			timeout = atof(optarg);
			break;
		case 'v':
			printf("radclient: $Id: radeapclient.c,v 1.7.4.5 2006/05/19 14:22:23 nbk Exp $ built on " __DATE__ " at " __TIME__ "\n");
			exit(0);
			break;
               case 'S':
		       fp = fopen(optarg, "r");
                       if (!fp) {
                               fprintf(stderr, "radclient: Error opening %s: %s\n",
                                       optarg, strerror(errno));
                               exit(1);
                       }
                       if (fgets(filesecret, sizeof(filesecret), fp) == NULL) {
                               fprintf(stderr, "radclient: Error reading %s: %s\n",
                                       optarg, strerror(errno));
                               exit(1);
                       }
		       fclose(fp);

                       /* truncate newline */
		       p = filesecret + strlen(filesecret) - 1;
		       while ((p >= filesecret) &&
			      (*p < ' ')) {
			       *p = '\0';
			       --p;
		       }

                       if (strlen(filesecret) < 2) {
                               fprintf(stderr, "radclient: Secret in %s is too short\n", optarg);
                               exit(1);
                       }
                       secret = filesecret;
		       break;
		case 'h':
		default:
			usage();
			break;
		}
	}
	argc -= (optind - 1);
	argv += (optind - 1);

	if ((argc < 3)  ||
	    ((secret == NULL) && (argc < 4))) {
		usage();
	}

	if (dict_init(radius_dir, RADIUS_DICTIONARY) < 0) {
		librad_perror("radclient");
		return 1;
	}

	if ((req = rad_alloc(1)) == NULL) {
		librad_perror("radclient");
		exit(1);
	}

#if 0
	{
		FILE *randinit;

		if((randinit = fopen("/dev/urandom", "r")) == NULL)
		{
			perror("/dev/urandom");
		} else {
			fread(randctx.randrsl, 256, 1, randinit);
			fclose(randinit);
		}
	}
	lrad_randinit(&randctx, 1);
#endif

	req->id = id;

	/*
	 *	Strip port from hostname if needed.
	 */
	if ((p = strchr(argv[1], ':')) != NULL) {
		*p++ = 0;
		port = atoi(p);
	}

	/*
	 *	See what kind of request we want to send.
	 */
	if (strcmp(argv[2], "auth") == 0) {
		if (port == 0) port = getport("radius");
		if (port == 0) port = PW_AUTH_UDP_PORT;
		req->code = PW_AUTHENTICATION_REQUEST;

	} else if (strcmp(argv[2], "acct") == 0) {
		if (port == 0) port = getport("radacct");
		if (port == 0) port = PW_ACCT_UDP_PORT;
		req->code = PW_ACCOUNTING_REQUEST;
		do_summary = 0;

	} else if (strcmp(argv[2], "status") == 0) {
		if (port == 0) port = getport("radius");
		if (port == 0) port = PW_AUTH_UDP_PORT;
		req->code = PW_STATUS_SERVER;

	} else if (strcmp(argv[2], "disconnect") == 0) {
		if (port == 0) port = PW_POD_UDP_PORT;
		req->code = PW_DISCONNECT_REQUEST;

	} else if (isdigit((int) argv[2][0])) {
		if (port == 0) port = getport("radius");
		if (port == 0) port = PW_AUTH_UDP_PORT;
		req->code = atoi(argv[2]);
	} else {
		usage();
	}

	/*
	 *	Ensure that the configuration is initialized.
	 */
	memset(&mainconfig, 0, sizeof(mainconfig));

	/*
	 *	Resolve hostname.
	 */
	req->dst_port = port;
	req->dst_ipaddr = ip_getaddr(argv[1]);
	if (req->dst_ipaddr == INADDR_NONE) {
		fprintf(stderr, "radclient: Failed to find IP address for host %s\n", argv[1]);
		exit(1);
	}

	/*
	 *	Add the secret.
	 */
	if (argv[3]) secret = argv[3];

	/*
	 *	Read valuepairs.
	 *	Maybe read them, from stdin, if there's no
	 *	filename, or if the filename is '-'.
	 */
	if (filename && (strcmp(filename, "-") != 0)) {
		fp = fopen(filename, "r");
		if (!fp) {
			fprintf(stderr, "radclient: Error opening %s: %s\n",
				filename, strerror(errno));
			exit(1);
		}
	} else {
		fp = stdin;
	}

	/*
	 *	Send request.
	 */
	if ((req->sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("radclient: socket: ");
		exit(1);
	}

	while(!filedone) {
		if(req->vps) pairfree(&req->vps);

		if ((req->vps = readvp2(fp, &filedone, "radeapclient:"))
		    == NULL) {
			break;
		}

		sendrecv_eap(req);
	}

	if(do_summary) {
		printf("\n\t   Total approved auths:  %d\n", totalapp);
		printf("\t     Total denied auths:  %d\n", totaldeny);
	}
	return 0;
}
