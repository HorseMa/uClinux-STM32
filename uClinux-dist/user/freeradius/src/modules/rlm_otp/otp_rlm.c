/*
 * otp_rlm.c
 * $Id: otp_rlm.c,v 1.19.2.9 2006/03/03 00:58:32 fcusack Exp $
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
 * Copyright 2000,2001,2002  The FreeRADIUS server project
 * Copyright 2001,2002  Google, Inc.
 * Copyright 2005,2006 TRI-D Systems, Inc.
 */

/*
 * STRONG WARNING SECTION:
 *
 * ANSI X9.9 has been withdrawn as a standard, due to the weakness of DES.
 * An attacker can learn the token's secret by observing two
 * challenge/response pairs.  See ANSI document X9 TG-24-1999
 * <URL:http://www.x9.org/docs/TG24_1999.pdf>.
 *
 * Please read the accompanying docs.
 */


#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <netinet/in.h>	/* htonl(), ntohl() */

#include "otp.h"
#ifdef FREERADIUS
#include <modules.h>
#endif

static const char rcsid[] = "$Id: otp_rlm.c,v 1.19.2.9 2006/03/03 00:58:32 fcusack Exp $";

/* Global data */
static unsigned char hmac_key[16];	/* to protect State attribute  */
static int ninstance = 0;		/* #instances, for global init */

/* A mapping of configuration file names to internal variables. */
static const CONF_PARSER module_config[] = {
  { "pwdfile", PW_TYPE_STRING_PTR, offsetof(otp_option_t, pwdfile),
    NULL, OTP_PWDFILE },
  { "lsmd_rp", PW_TYPE_STRING_PTR, offsetof(otp_option_t, lsmd_rp),
    NULL, OTP_LSMD_RP },
  { "challenge_prompt", PW_TYPE_STRING_PTR,offsetof(otp_option_t, chal_prompt),
    NULL, OTP_CHALLENGE_PROMPT },
  { "challenge_length", PW_TYPE_INTEGER, offsetof(otp_option_t, chal_len),
    NULL, "6" },
  { "challenge_delay", PW_TYPE_INTEGER, offsetof(otp_option_t, chal_delay),
    NULL, "30" },
  { "softfail", PW_TYPE_INTEGER, offsetof(otp_option_t, softfail),
    NULL, "5" },
  { "hardfail", PW_TYPE_INTEGER, offsetof(otp_option_t, hardfail),
    NULL, "0" },
  { "allow_sync", PW_TYPE_BOOLEAN, offsetof(otp_option_t, allow_sync),
    NULL, "yes" },
  { "fast_sync", PW_TYPE_BOOLEAN, offsetof(otp_option_t, fast_sync),
    NULL, "yes" },
  { "allow_async", PW_TYPE_BOOLEAN, offsetof(otp_option_t, allow_async),
    NULL, "no" },
  { "challenge_req", PW_TYPE_STRING_PTR, offsetof(otp_option_t, chal_req),
    NULL, OTP_CHALLENGE_REQ },
  { "resync_req", PW_TYPE_STRING_PTR, offsetof(otp_option_t, resync_req),
    NULL, OTP_RESYNC_REQ },
  { "prepend_pin", PW_TYPE_BOOLEAN, offsetof(otp_option_t, prepend_pin),
    NULL, "yes" },
  { "ewindow_size", PW_TYPE_INTEGER, offsetof(otp_option_t, ewindow_size),
    NULL, "0" },
  { "rwindow_size", PW_TYPE_INTEGER, offsetof(otp_option_t, rwindow_size),
    NULL, "0" },
  { "rwindow_delay", PW_TYPE_INTEGER, offsetof(otp_option_t, rwindow_delay),
    NULL, "60" },
  { "mschapv2_mppe", PW_TYPE_INTEGER,
    offsetof(otp_option_t, mschapv2_mppe_policy), NULL, "2" },
  { "mschapv2_mppe_bits", PW_TYPE_INTEGER,
    offsetof(otp_option_t, mschapv2_mppe_types), NULL, "2" },
  { "mschap_mppe", PW_TYPE_INTEGER,
    offsetof(otp_option_t, mschap_mppe_policy), NULL, "2" },
  { "mschap_mppe_bits", PW_TYPE_INTEGER,
    offsetof(otp_option_t, mschap_mppe_types), NULL, "2" },

  { "site_transform", PW_TYPE_BOOLEAN, offsetof(otp_option_t, site_transform),
    NULL, "yes" },
  { "debug", PW_TYPE_BOOLEAN, offsetof(otp_option_t, debug),
    NULL, "no" },

  { NULL, -1, 0, NULL, NULL }		/* end the list */
};


/* transform otp_pw_valid() return code into an rlm return code */
static int
otprc2rlmrc(int rc)
{
  switch (rc) {
    case OTP_RC_OK:                     return RLM_MODULE_OK;
    case OTP_RC_USER_UNKNOWN:           return RLM_MODULE_REJECT;
    case OTP_RC_AUTHINFO_UNAVAIL:       return RLM_MODULE_REJECT;
    case OTP_RC_AUTH_ERR:               return RLM_MODULE_REJECT;
    case OTP_RC_MAXTRIES:               return RLM_MODULE_USERLOCK;
    case OTP_RC_SERVICE_ERR:            return RLM_MODULE_FAIL;
    default:                            return RLM_MODULE_FAIL;
  }
}


/* per-instance initialization */
static int
otp_instantiate(CONF_SECTION *conf, void **instance)
{
  const char *log_prefix = OTP_MODULE_NAME;
  otp_option_t *opt;
  char *p;

  /* Set up a storage area for instance data. */
  opt = rad_malloc(sizeof(*opt));
  (void) memset(opt, 0, sizeof(*opt));

  /* If the configuration parameters can't be parsed, then fail. */
  if (cf_section_parse(conf, opt, module_config) < 0) {
    free(opt);
    return -1;
  }

  /* Onetime initialization. */
  if (!ninstance) {
    /* Generate a random key, used to protect the State attribute. */
    if (otp_get_random(-1, hmac_key, sizeof(hmac_key), log_prefix) == -1) {
      otp_log(OTP_LOG_ERR, "%s: %s: failed to obtain random data for hmac_key",
              log_prefix, __func__);
      free(opt);
      return -1;
    }

    /* Initialize the passcode encoding/checking functions. */
    otp_pwe_init();

    /*
     * Don't do this again.
     * Only the main thread instantiates and detaches instances,
     * so this does not need mutex protection.
     */
    ninstance++;
  }

  /* Verify ranges for those vars that are limited. */
  if ((opt->chal_len < 5) || (opt->chal_len > OTP_MAX_CHALLENGE_LEN)) {
    opt->chal_len = 6;
    otp_log(OTP_LOG_ERR,
            "%s: %s: invalid challenge_length, range 5-%d, using default of 6",
            log_prefix, __func__, OTP_MAX_CHALLENGE_LEN);
  }

  /* Enforce a single "%" sequence, which must be "%s" */
  p = strchr(opt->chal_prompt, '%');
  if ((p == NULL) || (p != strrchr(opt->chal_prompt, '%')) ||
      strncmp(p,"%s",2)) {
    free(opt->chal_prompt);
    opt->chal_prompt = strdup(OTP_CHALLENGE_PROMPT);
    otp_log(OTP_LOG_ERR,
            "%s: %s: invalid challenge_prompt, using default of \"%s\"",
            log_prefix, __func__, OTP_CHALLENGE_PROMPT);
  }

  if (opt->softfail < 0) {
    opt->softfail = 5;
    otp_log(OTP_LOG_ERR, "%s: %s: softfail must be at least 1 "
                         "(or 0 == infinite), using default of 5",
            log_prefix, __func__);
  }

  if (opt->hardfail < 0) {
    opt->hardfail = 0;
    otp_log(OTP_LOG_ERR, "%s: %s: hardfail must be at least 1 "
                         "(or 0 == infinite), using default of 0",
            log_prefix, __func__);
  }

  if (!opt->hardfail && opt->hardfail <= opt->softfail) {
    /*
     * This is noise if the admin leaves softfail alone, so it gets
     * the default value of 5, and sets hardfail <= to that ... but
     * in practice that will never happen.  Anyway, it is easily
     * overcome with a softfail setting of 0.
     *
     * This is because we can't tell the difference between a default
     * [softfail] value and an admin-configured one.
     */
    otp_log(OTP_LOG_ERR, "%s: %s: hardfail (%d) is less than softfail (%d), "
                         "effectively disabling softfail",
            log_prefix, __func__, opt->hardfail, opt->softfail);
  }

  if (opt->fast_sync && !opt->allow_sync) {
    opt->fast_sync = 0;
    otp_log(OTP_LOG_ERR, "%s: %s: fast_sync is yes, but allow_sync is no; "
                         "disabling fast_sync",
            log_prefix, __func__);
  }

  if (!opt->allow_sync && !opt->allow_async) {
    otp_log(OTP_LOG_ERR,
            "%s: %s: at least one of {allow_async, allow_sync} must be set",
            log_prefix, __func__);
    free(opt);
    return -1;
  }

  if ((opt->ewindow_size > OTP_MAX_EWINDOW_SIZE) ||
    (opt->ewindow_size < 0)) {
    opt->ewindow_size = 0;
    otp_log(OTP_LOG_ERR, "%s: %s: max ewindow_size is %d, using default of 0",
            log_prefix, __func__, OTP_MAX_EWINDOW_SIZE);
  }

  if (opt->rwindow_size && (opt->rwindow_size < opt->ewindow_size)) {
    opt->rwindow_size = 0;
    otp_log(OTP_LOG_ERR, "%s: %s: rwindow_size must be at least as large as "
                         "ewindow_size, using default of 0",
            log_prefix, __func__);
  }

  if (opt->rwindow_size && !opt->rwindow_delay) {
    opt->rwindow_size = 0;
    otp_log(OTP_LOG_ERR, "%s: %s: rwindow_size is non-zero, "
                         "but rwindow_delay is zero; disabling rwindow",
            log_prefix, __func__);
  }

  if ((opt->mschapv2_mppe_policy > 2) || (opt->mschapv2_mppe_policy < 0)) {
    opt->mschapv2_mppe_policy = 2;
    otp_log(OTP_LOG_ERR,
            "%s: %s: invalid value for mschapv2_mppe, using default of 2",
            log_prefix, __func__);
  }

  if ((opt->mschapv2_mppe_types > 2) || (opt->mschapv2_mppe_types < 0)) {
    opt->mschapv2_mppe_types = 2;
    otp_log(OTP_LOG_ERR,
            "%s: %s: invalid value for mschapv2_mppe_bits, using default of 2",
            log_prefix, __func__);
  }

  if ((opt->mschap_mppe_policy > 2) || (opt->mschap_mppe_policy < 0)) {
    opt->mschap_mppe_policy = 2;
    otp_log(OTP_LOG_ERR,
            "%s: %s: invalid value for mschap_mppe, using default of 2",
            log_prefix, __func__);
  }

  if (opt->mschap_mppe_types != 2) {
    opt->mschap_mppe_types = 2;
    otp_log(OTP_LOG_ERR,
            "%s: %s: invalid value for mschap_mppe_bits, using default of 2",
            log_prefix, __func__);
  }

  /* set the instance name (for use with authorize()) */
  opt->name = cf_section_name2(conf);
  if (!opt->name)
    opt->name = cf_section_name1(conf);
  if (!opt->name) {
    otp_log(OTP_LOG_CRIT, "%s: %s: no instance name (this can't happen)",
            log_prefix, __func__);
    free(opt);
    return -1;
  }

  /* set debug opt for portable debug output (see DEBUG definition) */
  if (debug_flag)
    opt->debug = 1;

  *instance = opt;
  return 0;
}


/* Generate a challenge to be presented to the user. */
static int
otp_authorize(void *instance, REQUEST *request)
{
  otp_option_t *inst = (otp_option_t *) instance;
  const char *log_prefix = OTP_MODULE_NAME;

  char challenge[OTP_MAX_CHALLENGE_LEN + 1];	/* +1 for '\0' terminator */
  char *state;
  int auth_type_found;
  int32_t sflags = 0; /* flags for state */

  struct otp_pwe_cmp_t data = {
    .request		= request,
    .inst		= inst,
    .returned_vps	= NULL
  };

  /* Early exit if Auth-Type != inst->name */
  {
    VALUE_PAIR *vp;

    auth_type_found = 0;
    if ((vp = pairfind(request->config_items, PW_AUTHTYPE)) != NULL) {
      auth_type_found = 1;
      if (strcmp(vp->strvalue, inst->name))
        return RLM_MODULE_NOOP;
    }
  }

  /* The State attribute will be present if this is a response. */
  if (pairfind(request->packet->vps, PW_STATE) != NULL) {
    DEBUG("rlm_otp: autz: Found response to Access-Challenge");
    return RLM_MODULE_OK;
  }

  /* User-Name attribute required. */
  if (!request->username) {
    otp_log(OTP_LOG_AUTH,
            "%s: %s: Attribute \"User-Name\" required for authentication.",
            log_prefix, __func__);
    return RLM_MODULE_INVALID;
  }

  if ((data.pwattr = otp_pwe_present(request, log_prefix)) == 0) {
    otp_log(OTP_LOG_AUTH, "%s: %s: Attribute \"User-Password\" "
                          "or equivalent required for authentication.",
            log_prefix, __func__);
    return RLM_MODULE_INVALID;
  }

  /* fast_sync mode (challenge only if requested) */
  if (inst->fast_sync) {
    if ((!otp_pwe_cmp(&data, inst->resync_req, log_prefix) &&
        /* Set a bit indicating resync */ (sflags |= htonl(1))) ||
        !otp_pwe_cmp(&data, inst->chal_req, log_prefix)) {
      /*
       * Generate a challenge if requested.  Note that we do this
       * even if configuration doesn't allow async mode.
       */
      DEBUG("rlm_otp: autz: fast_sync challenge requested");
      goto gen_challenge;

    } else {
      /* Otherwise, this is the token sync response. */
      if (!auth_type_found)
        pairadd(&request->config_items, pairmake("Auth-Type", "otp", T_OP_EQ));
        return RLM_MODULE_OK;

    }
  } /* if (fast_sync && card supports sync mode) */

gen_challenge:
  /* Set the resync bit by default if the user can't choose. */
  if (!inst->fast_sync)
    sflags |= htonl(1);

  /* Generate a random challenge. */
  if (otp_async_challenge(-1, challenge, inst->chal_len, log_prefix) == -1) {
    otp_log(OTP_LOG_ERR, "%s: %s: failed to obtain random challenge",
            log_prefix, __func__);
    return RLM_MODULE_FAIL;
  }

  /*
   * Create the State attribute, which will be returned to us along with
   * the response.  We will need this to verify the response.  It must
   * be hmac protected to prevent insertion of arbitrary State by an
   * inside attacker.  If we won't actually use the State (server config
   * doesn't allow async), we just use a trivial State.  We always create
   * at least a trivial State, so otp_authorize() can quickly pass on to
   * otp_authenticate().
   */
  if (inst->allow_async) {
    int32_t now = htonl(time(NULL));	/* low-order 32 bits on LP64 */

    if (otp_gen_state(&state, NULL, challenge, inst->chal_len, sflags,
                      now, hmac_key) != 0) {
      otp_log(OTP_LOG_ERR, "%s: %s: failed to generate state",
              log_prefix, __func__);
      return RLM_MODULE_FAIL;
    }
  } else {
    state = rad_malloc(5);
    /* a non-NUL byte, so that Cisco (see otp_gen_state()) returns it */
    (void) sprintf(state, "0x01");
  }
  pairadd(&request->reply->vps, pairmake("State", state, T_OP_EQ));
  free(state);

  /* Add the challenge to the reply. */
  {
    char *u_challenge;	/* challenge with addt'l presentation text */

    u_challenge = rad_malloc(strlen(inst->chal_prompt) +
                             OTP_MAX_CHALLENGE_LEN + 1);
    (void) sprintf(u_challenge, inst->chal_prompt, challenge);
    pairadd(&request->reply->vps,
            pairmake("Reply-Message", u_challenge, T_OP_EQ));
    free(u_challenge);
  }

  /*
   * Mark the packet as an Access-Challenge packet.
   * The server will take care of sending it to the user.
   */
  request->reply->code = PW_ACCESS_CHALLENGE;
  DEBUG("rlm_otp: Sending Access-Challenge.");

  /* TODO: support config-specific auth-type */
  if (!auth_type_found)
    pairadd(&request->config_items, pairmake("Auth-Type", "otp", T_OP_EQ));
  return RLM_MODULE_HANDLED;
}


/* Verify the response entered by the user. */
static int
otp_authenticate(void *instance, REQUEST *request)
{
  otp_option_t *inst = (otp_option_t *) instance;
  const char *log_prefix = OTP_MODULE_NAME;

  char *username;
  int rc;
  int resync = 0;	/* resync flag for async mode */

  unsigned char challenge[OTP_MAX_CHALLENGE_LEN];	/* cf. authorize() */
  VALUE_PAIR *add_vps = NULL;

  struct otp_pwe_cmp_t data = {
    .request		= request,
    .inst		= inst,
    .returned_vps	= &add_vps
  };

  challenge[0] = '\0';	/* initialize for otp_pw_valid() */

  /* User-Name attribute required. */
  if (!request->username) {
    otp_log(OTP_LOG_AUTH,
            "%s: %s: Attribute \"User-Name\" required for authentication.",
            log_prefix, __func__);
    return RLM_MODULE_INVALID;
  }
  username = request->username->strvalue;

  if ((data.pwattr = otp_pwe_present(request, log_prefix)) == 0) {
    otp_log(OTP_LOG_AUTH, "%s: %s: Attribute \"User-Password\" "
                          "or equivalent required for authentication.",
            log_prefix, __func__);
    return RLM_MODULE_INVALID;
  }

  /* Add a message to the auth log. */
  pairadd(&request->packet->vps, pairmake("Module-Failure-Message",
                                          OTP_MODULE_NAME, T_OP_EQ));
  pairadd(&request->packet->vps, pairmake("Module-Success-Message",
                                          OTP_MODULE_NAME, T_OP_EQ));

  /* Retrieve the challenge (from State attribute). */
  {
    VALUE_PAIR	*vp;
    unsigned char	*state;
    unsigned char	*raw_state;
    unsigned char	*rad_state;
    int32_t		sflags = 0; 	/* state flags           */
    int32_t		then;		/* state timestamp       */
    int			e_length;	/* expected State length */

    if ((vp = pairfind(request->packet->vps, PW_STATE)) != NULL) {
      /* set expected State length */
      if (inst->allow_async)
        e_length = inst->chal_len * 2 + 8 + 8 + 32; /* see otp_gen_state() */
      else
        e_length = 1;

      if (vp->length != e_length) {
        otp_log(OTP_LOG_AUTH, "%s: %s: bad state for [%s]: length",
                log_prefix, __func__, username);
        return RLM_MODULE_INVALID;
      }

      if (inst->allow_async) {
        /*
         * Verify the state.
         */

        rad_state = rad_malloc(e_length + 1);
        raw_state = rad_malloc(e_length / 2);

        /* ASCII decode */
        (void) memcpy(rad_state, vp->strvalue, vp->length);
        rad_state[e_length] = '\0';
        (void) otp_keystring2keyblock(rad_state, raw_state);
        free(rad_state);
        
        /* extract data from State */
        (void) memcpy(challenge, raw_state, inst->chal_len);
        (void) memcpy(&sflags, raw_state + inst->chal_len, 4);
        (void) memcpy(&then, raw_state + inst->chal_len + 4, 4);
        free(raw_state);

        /* generate new state from returned input data */
        if (otp_gen_state(NULL, &state, challenge, inst->chal_len,
                          sflags, then, hmac_key) != 0) {
          otp_log(OTP_LOG_ERR, "%s: %s: failed to generate state",
                  log_prefix, __func__);
          return RLM_MODULE_FAIL;
        }
        /* compare generated state against returned state to verify hmac */
        if (memcmp(state, vp->strvalue, vp->length)) {
          otp_log(OTP_LOG_AUTH, "%s: %s: bad state for [%s]: hmac",
                  log_prefix, __func__, username);
          free(state);
          return RLM_MODULE_REJECT;
        }
        free(state);

        /* State is valid, but check expiry. */
        then = ntohl(then);
        if (time(NULL) - then > inst->chal_delay) {
          otp_log(OTP_LOG_AUTH, "%s: %s: bad state for [%s]: expired",
                  log_prefix, __func__, username);
          return RLM_MODULE_REJECT;
        }
        resync = ntohl(sflags) & 1;
      } /* if (State should have been protected) */
    } /* if (State present) */
  } /* code block */

  /* do it */
  rc = otprc2rlmrc(otp_pw_valid(username, challenge, NULL, resync, inst,
                                otp_pwe_cmp, &data, log_prefix));

  /* Handle any vps returned from otp_pwe_cmp(). */
  if (rc == RLM_MODULE_OK) {
    pairadd(&request->reply->vps, add_vps);
  } else {
    pairfree(&add_vps);
  }
  return rc;
}


/* per-instance destruction */
static int
otp_detach(void *instance)
{
  otp_option_t *inst = (otp_option_t *) instance;

  free(inst->pwdfile);
  free(inst->lsmd_rp);
  free(inst->chal_prompt);
  free(inst->chal_req);
  free(inst->resync_req);
  free(instance);
  /*
   * Only the main thread instantiates and detaches instances,
   * so this does not need mutex protection.
   */
  if (--ninstance == 0)
    memset(hmac_key, 0, sizeof(hmac_key));

  return 0;
}


/*
 *	If the module needs to temporarily modify it's instantiation
 *	data, the type should be changed to RLM_TYPE_THREAD_UNSAFE.
 *	The server will then take care of ensuring that the module
 *	is single-threaded.
 */
module_t rlm_otp = {
  "otp",
  RLM_TYPE_THREAD_SAFE,		/* type */
  NULL,				/* initialization */
  otp_instantiate,		/* instantiation */
  {
    otp_authenticate,		/* authentication */
    otp_authorize,		/* authorization */
    NULL,			/* preaccounting */
    NULL,			/* accounting */
    NULL,			/* checksimul */
    NULL,			/* pre-proxy */
    NULL,			/* post-proxy */
    NULL			/* post-auth */
  },
  otp_detach,			/* detach */
  NULL,				/* destroy */
};
