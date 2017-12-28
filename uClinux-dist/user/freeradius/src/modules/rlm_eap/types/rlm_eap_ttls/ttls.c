/*
 * rlm_eap_ttls.c  contains the interfaces that are called from eap
 *
 * Version:     $Id: ttls.c,v 1.17.2.2.2.1 2006/02/06 16:23:57 nbk Exp $
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
 *   Copyright 2003 Alan DeKok <aland@freeradius.org>
 */
#include "eap_ttls.h"

/*
 *    0                   1                   2                   3
 *    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |                           AVP Code                            |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |V M r r r r r r|                  AVP Length                   |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |                        Vendor-ID (opt)                        |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |    Data ...
 *   +-+-+-+-+-+-+-+-+
 */

/*
 *	Verify that the diameter packet is valid.
 */
static int diameter_verify(const uint8_t *data, unsigned int data_len)
{
	uint32_t attr;
	uint32_t length;
	unsigned int offset;
	unsigned int data_left = data_len;

	while (data_left > 0) {
		rad_assert(data_left <= data_len);
		memcpy(&attr, data, sizeof(attr));
		data += 4;
		attr = ntohl(attr);
		if (attr > 255) {
			DEBUG2("  rlm_eap_ttls:  Non-RADIUS attribute in tunneled authentication is not supported");
			return 0;
		}

		memcpy(&length, data , sizeof(length));
		data += 4;
		length = ntohl(length);

		/*
		 *	A "vendor" flag, with a vendor ID of zero,
		 *	is equivalent to no vendor.  This is stupid.
		 */
		offset = 8;
		if ((length & (1 << 31)) != 0) {
			int attribute;
			uint32_t vendor;
			DICT_ATTR *da;

			memcpy(&vendor, data, sizeof(vendor));
			vendor = ntohl(vendor);

			if (vendor > 65535) {
				DEBUG2("  rlm_eap_ttls: Vendor codes larger than 65535 are not supported");
				return 0;
			}

			attribute = (vendor << 16) | attr;

			da = dict_attrbyvalue(attribute);

			/*
			 *	SHOULD check ((length & (1 << 30)) != 0)
			 *	for the mandatory bit.
			 */
			if (!da) {
				DEBUG2("  rlm_eap_ttls: Fatal! Vendor %u, Attribute %u was not found in our dictionary. ",
				       vendor, attr);
				return 0;
			}

			data += 4; /* skip the vendor field */
			offset += 4; /* offset to value field */
		}

		/*
		 *	Ignore the M bit.  We support all RADIUS attributes...
		 */

		/*
		 *	Get the length.  If it's too big, die.
		 */
		length &= 0x00ffffff;

		/*
		 *	Too short or too long is bad.
		 *
		 *	FIXME: EAP-Message
		 */
		if (length < offset) {
			DEBUG2("  rlm_eap_ttls: Tunneled attribute %d is too short (%d)to contain anything useful.", attr, length);
			return 0;
		}

		if (length > (MAX_STRING_LEN + 8)) {
			DEBUG2("  rlm_eap_ttls: Tunneled attribute %d is too long (%d) to pack into a RADIUS attribute.", attr, length);
			return 0;
		}
		    
		if (length > data_left) {
			DEBUG2("  rlm_eap_ttls: Tunneled attribute %d is longer than room left in the packet (%d > %d).", attr, length, data_left);
			return 0;
		}

		/*
		 *	Check for broken implementations, which don't
		 *	pad the AVP to a 4-octet boundary.
		 */
		if (data_left == length) break;

		/*
		 *	The length does NOT include the padding, so
		 *	we've got to account for it here by rounding up
		 *	to the nearest 4-byte boundary.
		 */
		length += 0x03;
		length &= ~0x03;

		/*
		 *	If the rest of the diameter packet is larger than
		 *	this attribute, continue.
		 *
		 *	Otherwise, if the attribute over-flows the end
		 *	of the packet, die.
		 */
		if (data_left < length) {
			DEBUG2("  rlm_eap_ttls: ERROR! Diameter attribute overflows packet!");
			return 0;
		}
	
		/*
		 *	Check again for equality, now that we're padded
		 *	length to a multiple of 4 octets.
		 */
		if (data_left == length) break;

		/*
		 *	data_left > length, continue.
		 */
		data_left -= length;
		data += length - offset;
	}

	/*
	 *	We got this far.  It looks OK.
	 */
	return 1;
}


/*
 *	Convert diameter attributes to our VALUE_PAIR's
 */
static VALUE_PAIR *diameter2vp(SSL *ssl,
			       const uint8_t *data, unsigned int data_len)
{
	uint32_t	attr;
	uint32_t	length;
	unsigned int	offset;
	int		size;
	unsigned int	data_left = data_len;
	VALUE_PAIR	*first = NULL;
	VALUE_PAIR	**last = &first;
	VALUE_PAIR	*vp;

	while (data_left > 0) {
		rad_assert(data_left <= data_len);
		memcpy(&attr, data, sizeof(attr));
		data += 4;
		attr = ntohl(attr);

		memcpy(&length, data, sizeof(length));
		data += 4;
		length = ntohl(length);

		/*
		 *	Ignore the M bit.  We support all RADIUS attributes...
		 */

		/*
		 *	A "vendor" flag, with a vendor ID of zero,
		 *	is equivalent to no vendor.  This is stupid.
		 */
		offset = 8;
		if ((length & (1 << 31)) != 0) {
			uint32_t vendor;

			memcpy(&vendor, data, sizeof(vendor));
			vendor = ntohl(vendor);

			attr |= (vendor << 16);

			data += 4; /* skip the vendor field, it's zero */
			offset += 4; /* offset to value field */
		}

		/*
		 *	Get the length.
		 */
		length &= 0x00ffffff;

		/*
		 *	diameter code + length, and it must fit in
		 *	a VALUE_PAIR.
		 */
		rad_assert(length <= (offset + MAX_STRING_LEN));

		/*
		 *	Get the size of the value portion of the
		 *	attribute.
		 */
		size = length - offset;

		/*
		 *	Create it.
		 */
		vp = paircreate(attr, PW_TYPE_OCTETS);
		if (!vp) {
			DEBUG2("  rlm_eap_ttls: Failure in creating VP");
			pairfree(&first);
			return NULL;
		}

		/*
		 *	If it's a type from our dictionary, then
		 *	we need to put the data in a relevant place.
		 */
		switch (vp->type) {
		case PW_TYPE_INTEGER:
		case PW_TYPE_DATE:
			if (size != vp->length) {
				DEBUG2("  rlm_eap_ttls: Invalid length attribute %d",
				       attr);
				pairfree(&first);
				return NULL;
			}
			memcpy(&vp->lvalue, data, vp->length);
			
			/*
			 *	Stored in host byte order: change it.
			 */
			vp->lvalue = ntohl(vp->lvalue);
			break;
			
		case PW_TYPE_IPADDR:
			if (size != vp->length) {
				DEBUG2("  rlm_eap_ttls: Invalid length attribute %d",
				       attr);
				pairfree(&first);
				return NULL;
			}
		  memcpy(&vp->lvalue, data, vp->length);
		  
		  /*
		   *	Stored in network byte order: don't change it.
		   */
		  break;

		  /*
		   *	String, octet, etc.  Copy the data from the
		   *	value field over verbatim.
		   *
		   *	FIXME: Ipv6 attributes ?
		   *
		   */
		default:
			vp->length = size;
			memcpy(vp->strvalue, data, vp->length);
			break;
		}

		/*
		 *	User-Password is NUL padded to a multiple
		 *	of 16 bytes.  Let's chop it to something
		 *	more reasonable.
		 *
		 *	NOTE: This means that the User-Password
		 *	attribute CANNOT EVER have embedded zeros in it!
		 */
		switch (vp->attribute) {
		case PW_USER_PASSWORD:
			rad_assert(vp->length <= 128); /* RFC requirements */

			/*
			 *	If the password is exactly 16 octets,
			 *	it won't be zero-terminated.
			 */
			vp->strvalue[vp->length] = '\0';
			vp->length = strlen(vp->strvalue);
			break;

			/*
			 *	Ensure that the client is using the
			 *	correct challenge.  This weirdness is
			 *	to protect against against replay
			 *	attacks, where anyone observing the
			 *	CHAP exchange could pose as that user,
			 *	by simply choosing to use the same
			 *	challenge.
			 *
			 *	By using a challenge based on
			 *	information from the current session,
			 *	we can guarantee that the client is
			 *	not *choosing* a challenge.
			 *
			 *	We're a little forgiving in that we
			 *	have loose checks on the length, and
			 *	we do NOT check the Id (first octet of
			 *	the response to the challenge)
			 *
			 *	But if the client gets the challenge correct,
			 *	we're not too worried about the Id.
			 */
		case PW_CHAP_CHALLENGE:
		case PW_MSCHAP_CHALLENGE:
			if ((vp->length < 8) ||
			    (vp->length > 16)) {
				DEBUG2("  TTLS: Tunneled challenge has invalid length");
				pairfree(&first);
				return NULL;

			} else {
				int i;
				uint8_t	challenge[16];

				eapttls_gen_challenge(ssl, challenge,
						      sizeof(challenge));

				for (i = 0; i < vp->length; i++) {
					if (challenge[i] != vp->strvalue[i]) {
						DEBUG2("  TTLS: Tunneled challenge is incorrect");
						pairfree(&first);
						return NULL;
					}
				}
			}
			break;

		default:
			break;
		} /* switch over checking/re-writing of attributes. */

		/*
		 *	Update the list.
		 */
		*last = vp;
		last = &(vp->next);

		/*
		 *	Catch non-aligned attributes.
		 */
		if (data_left == length) break;

		/*
		 *	The length does NOT include the padding, so
		 *	we've got to account for it here by rounding up
		 *	to the nearest 4-byte boundary.
		 */
		length += 0x03;
		length &= ~0x03;

		rad_assert(data_left >= length);
		data_left -= length;
		data += length - offset; /* already updated */
	}

	/*
	 *	We got this far.  It looks OK.
	 */
	return first;
}

/*
 *	Convert VALUE_PAIR's to diameter attributes, and write them
 *	to an SSL session.
 *
 *	The ONLY VALUE_PAIR's which may be passed to this function
 *	are ones which can go inside of a RADIUS (i.e. diameter)
 *	packet.  So no server-configuration attributes, or the like.
 */
static int vp2diameter(tls_session_t *tls_session, VALUE_PAIR *first)
{
	/*
	 *	RADIUS packets are no more than 4k in size, so if
	 *	we've got more than 4k of data to write, it's very
	 *	bad.
	 */
	uint8_t		buffer[4096];
	uint8_t		*p;
	uint32_t	attr;
	uint32_t	length;
	uint32_t	vendor;
	size_t		total;
	VALUE_PAIR	*vp;

	p = buffer;
	total = 0;

	for (vp = first; vp != NULL; vp = vp->next) {
		/*
		 *	Too much data: die.
		 */
		if ((total + vp->length + 12) >= sizeof(buffer)) {
			DEBUG2("  TTLS output buffer is full!");
			return 0;
		}

		/*
		 *	Hmm... we don't group multiple EAP-Messages
		 *	together.  Maybe we should...
		 */

		/*
		 *	Length is no more than 253, due to RADIUS
		 *	issues.
		 */
		length = vp->length;
		vendor = (vp->attribute >> 16) & 0xffff;
		if (vendor != 0) {
			attr = vp->attribute & 0xffff;
			length |= (1 << 31);
		} else {
			attr = vp->attribute;
		}

		/*
		 *	Hmm... set the M bit for all attributes?
		 */
		length |= (1 << 30);

		attr = ntohl(attr);

		memcpy(p, &attr, sizeof(attr));
		p += 4;
		total += 4;

		length += 8;	/* includes 8 bytes of attr & length */

		if (vendor != 0) {
			length += 4; /* include 4 bytes of vendor */

			length = ntohl(length);
			memcpy(p, &length, sizeof(length));
			p += 4;
			total += 4;

			vendor = ntohl(vendor);
			memcpy(p, &vendor, sizeof(vendor));
			p += 4;
			total += 4;
		} else {
			length = ntohl(length);
			memcpy(p, &length, sizeof(length));
			p += 4;
			total += 4;
		}

		switch (vp->type) {
		case PW_TYPE_INTEGER:
		case PW_TYPE_DATE:
			attr = ntohl(vp->lvalue); /* stored in host order */
			memcpy(p, &attr, sizeof(attr));
			length = 4;
			break;

		case PW_TYPE_IPADDR:
			attr = vp->lvalue; /* stored in network order */
			memcpy(p, &attr, sizeof(attr));
			length = 4;
			break;

		case PW_TYPE_STRING:
		case PW_TYPE_OCTETS:
		default:
			memcpy(p, vp->strvalue, vp->length);
			length = vp->length;
			break;
		}

		/*
		 *	Skip to the end of the data.
		 */
		p += length;
		total += length;

		/*
		 *	Align the data to a multiple of 4 bytes.
		 */
		if ((total & 0x03) != 0) {
			unsigned int i;

			length = 4 - (total & 0x03);
			for (i = 0; i < length; i++) {
				*p = '\0';
				p++;
				total++;
			}
		}
	} /* loop over the VP's to write. */

	/*
	 *	Write the data in the buffer to the SSL session.
	 */
	if (total > 0) {
#ifndef NDEBUG
		unsigned int i;

		if (debug_flag > 2) {
			for (i = 0; i < total; i++) {
				if ((i & 0x0f) == 0) printf("  TTLS tunnel data out %04x: ", i);

				printf("%02x ", buffer[i]);

				if ((i & 0x0f) == 0x0f) printf("\n");
			}
			if ((total & 0x0f) != 0) printf("\n");
		}
#endif

		(tls_session->record_plus)(&tls_session->clean_in, buffer, total);

		/*
		 *	FIXME: Check the return code.
		 */
		tls_handshake_send(tls_session);
	}

	/*
	 *	Everything's OK.
	 */
	return 1;
}

/*
 *	Use a reply packet to determine what to do.
 */
static int process_reply(EAP_HANDLER *handler, tls_session_t *tls_session,
			 REQUEST *request, RADIUS_PACKET *reply)
{
	int rcode = RLM_MODULE_REJECT;
	VALUE_PAIR *vp;
	ttls_tunnel_t *t = tls_session->opaque;

	handler = handler;	/* -Wunused */

	/*
	 *	If the response packet was Access-Accept, then
	 *	we're OK.  If not, die horribly.
	 *
	 *	FIXME: Take MS-CHAP2-Success attribute, and
	 *	tunnel it back to the client, to authenticate
	 *	ourselves to the client.
	 *
	 *	FIXME: If we have an Access-Challenge, then
	 *	the Reply-Message is tunneled back to the client.
	 *
	 *	FIXME: If we have an EAP-Message, then that message
	 *	must be tunneled back to the client.
	 *
	 *	FIXME: If we have an Access-Challenge with a State
	 *	attribute, then do we tunnel that to the client, or
	 *	keep track of it ourselves?
	 *
	 *	FIXME: EAP-Messages can only start with 'identity',
	 *	NOT 'eap start', so we should check for that....
	 */
	switch (reply->code) {
	case PW_AUTHENTICATION_ACK:
		DEBUG2("  TTLS: Got tunneled Access-Accept");

		rcode = RLM_MODULE_OK;

		/*
		 *	MS-CHAP2-Success means that we do NOT return
		 *	an Access-Accept, but instead tunnel that
		 *	attribute to the client, and keep going with
		 *	the TTLS session.  Once the client accepts
		 *	our identity, it will respond with an empty
		 *	packet, and we will send EAP-Success.
		 */
		vp = NULL;
		pairmove2(&vp, &reply->vps, PW_MSCHAP2_SUCCESS);
		if (vp) {
#if 1
			/*
			 *	FIXME: Tunneling MS-CHAP2-Success causes
			 *	the only client we have access to, to die.
			 *
			 *	We don't want that...
			 */
			pairfree(&vp);
#else
			DEBUG2("  TTLS: Got MS-CHAP2-Success, tunneling it to the client in a challenge.");
			rcode = RLM_MODULE_HANDLED;
			t->authenticated = TRUE;
#endif
		} else { /* no MS-CHAP2-Success */
			/*
			 *	Can only have EAP-Message if there's
			 *	no MS-CHAP2-Success.  (FIXME: EAP-MSCHAP?)
			 *
			 *	We also do NOT tunnel the EAP-Success
			 *	attribute back to the client, as the client
			 *	can figure it out, from the non-tunneled
			 *	EAP-Success packet.
			 */
			pairmove2(&vp, &reply->vps, PW_EAP_MESSAGE);
			pairfree(&vp);
		}

		/*
		 *	Handle the ACK, by tunneling any necessary reply
		 *	VP's back to the client.
		 */
		if (vp) {
			vp2diameter(tls_session, vp);
			pairfree(&vp);
		}

		/*
		 *	If we've been told to use the attributes from
		 *	the reply, then do so.
		 *
		 *	WARNING: This may leak information about the
		 *	tunneled user!
		 */
		if (t->use_tunneled_reply) {
			pairdelete(&reply->vps, PW_PROXY_STATE);
			pairadd(&request->reply->vps, reply->vps);
			reply->vps = NULL;
		}
		break;


	case PW_AUTHENTICATION_REJECT:
		DEBUG2("  TTLS: Got tunneled Access-Reject");
		rcode = RLM_MODULE_REJECT;
		break;

		/*
		 *	Handle Access-Challenge, but only if we
		 *	send tunneled reply data.  This is because
		 *	an Access-Challenge means that we MUST tunnel
		 *	a Reply-Message to the client.
		 */
	case PW_ACCESS_CHALLENGE:
		DEBUG2("  TTLS: Got tunneled Access-Challenge");

		/*
		 *	Keep the State attribute, if necessary.
		 *
		 *	Get rid of the old State, too.
		 */
		pairfree(&t->state);
		pairmove2(&t->state, &reply->vps, PW_STATE);

		/*
		 *	We should really be a bit smarter about this,
		 *	and move over only those attributes which
		 *	are relevant to the authentication request,
		 *	but that's a lot more work, and this "dumb"
		 *	method works in 99.9% of the situations.
		 */
		vp = NULL;
		pairmove2(&vp, &reply->vps, PW_EAP_MESSAGE);

		/*
		 *	There MUST be a Reply-Message in the challenge,
		 *	which we tunnel back to the client.
		 *
		 *	If there isn't one in the reply VP's, then
		 *	we MUST create one, with an empty string as
		 *	it's value.
		 */
		pairmove2(&vp, &reply->vps, PW_REPLY_MESSAGE);

		/*
		 *	Handle the ACK, by tunneling any necessary reply
		 *	VP's back to the client.
		 */
		if (vp) {
			vp2diameter(tls_session, vp);
			pairfree(&vp);
		}
		rcode = RLM_MODULE_HANDLED;
		break;

	default:
		DEBUG2("  TTLS: Unknown RADIUS packet type %d: rejecting tunneled user", reply->code);
		rcode = RLM_MODULE_INVALID;
		break;
	}

	return rcode;
}


/*
 *	Do post-proxy processing,
 */
static int eapttls_postproxy(EAP_HANDLER *handler, void *data)
{
	int rcode;
	tls_session_t *tls_session = (tls_session_t *) data;
	REQUEST *fake;

	DEBUG2("  TTLS: Passing reply from proxy back into the tunnel.");

	/*
	 *	If there was a fake request associated with the proxied
	 *	request, do more processing of it.
	 */
	fake = (REQUEST *) request_data_get(handler->request,
					    handler->request->proxy,
					    REQUEST_DATA_EAP_MSCHAP_TUNNEL_CALLBACK);
	
	/*
	 *	Do the callback, if it exists, and if it was a success.
	 */
	if (fake && (handler->request->proxy_reply->code == PW_AUTHENTICATION_ACK)) {
		VALUE_PAIR *vp;
		REQUEST *request = handler->request;

		/*
		 *	Terrible hacks.
		 */
		rad_assert(fake->packet == NULL);
		fake->packet = request->proxy;
		request->proxy = NULL;

		rad_assert(fake->reply == NULL);
		fake->reply = request->proxy_reply;
		request->proxy_reply = NULL;

		/*
		 *	Perform a post-auth stage for the tunneled
		 *	session.
		 */
		fake->options &= ~RAD_REQUEST_OPTION_PROXY_EAP;
		rcode = rad_postauth(fake);
		DEBUG2("  POST-AUTH %d", rcode);

#ifndef NDEBUG
		if (debug_flag > 0) {
			printf("  TTLS: Final reply from tunneled session code %d\n",
			       fake->reply->code);
			
			for (vp = fake->reply->vps; vp != NULL; vp = vp->next) {
				putchar('\t');vp_print(stdout, vp);putchar('\n');
			}
		}
#endif

		/*
		 *	Terrible hacks.
		 */
		request->proxy = fake->packet;
		fake->packet = NULL;
		request->proxy_reply = fake->reply;
		fake->reply = NULL;

		/*
		 *	And we're done with this request.
		 */

		switch (rcode) {
                case RLM_MODULE_FAIL:
			request_free(&fake);
			eaptls_fail(handler->eap_ds, 0);
			return 0;
			break;
			
                default:  /* Don't Do Anything */
			DEBUG2(" TTLS: Got reply %d",
			       request->proxy_reply->code);
			break;
		}	
	}
	request_free(&fake);	/* robust if fake == NULL */

	/*
	 *	Process the reply from the home server.
	 */
	rcode = process_reply(handler, tls_session, handler->request,
			      handler->request->proxy_reply);

	/*
	 *	The proxy code uses the reply from the home server as
	 *	the basis for the reply to the NAS.  We don't want that,
	 *	so we toss it, after we've had our way with it.
	 */
	pairfree(&handler->request->proxy_reply->vps);

	switch (rcode) {
	case RLM_MODULE_REJECT:
		DEBUG2("  TTLS: Reply was rejected");
		break;

	case RLM_MODULE_HANDLED:
		DEBUG2("  TTLS: Reply was handled");
		eaptls_request(handler->eap_ds, tls_session);
		return 1;

	case RLM_MODULE_OK:
		DEBUG2("  TTLS: Reply was OK");
		eaptls_success(handler->eap_ds, 0);
		eaptls_gen_mppe_keys(&handler->request->reply->vps,
				     tls_session->ssl,
				     "ttls keying material");
		return 1;

	default:
		DEBUG2("  TTLS: Reply was unknown.");
		break;
	}

	eaptls_fail(handler->eap_ds, 0);
	return 0;
}


/*
 *	Free a request.
 */
static void my_request_free(void *data)
{
	REQUEST *request = (REQUEST *)data;

	request_free(&request);
}


/*
 *	Process the "diameter" contents of the tunneled data.
 */
int eapttls_process(EAP_HANDLER *handler, tls_session_t *tls_session)
{
	int err;
	int rcode = PW_AUTHENTICATION_REJECT;
	REQUEST *fake;
	VALUE_PAIR *vp;
	ttls_tunnel_t *t;
	const uint8_t *data;
	unsigned int data_len;
	char buffer[1024];
	REQUEST *request = handler->request;

	/*
	 *	Grab the dirty data, and copy it to our buffer.
	 *
	 *	I *really* don't like these 'record_t' things...
	 */
	data_len = (tls_session->record_minus)(&tls_session->dirty_in, buffer, sizeof(buffer));
	data = buffer;

	/*
	 *	Write the data from the dirty buffer (i.e. packet
	 *	data) into the buffer which we will give to SSL for
	 *	decoding.
	 *
	 *	Some of this code COULD technically go into the TLS
	 *	module, in eaptls_process(), where it returns EAPTLS_OK.
	 *
	 *	Similarly, the writing of data to the SSL context could
	 *	go there, too...
	 */
	BIO_write(tls_session->into_ssl, buffer, data_len);
	(tls_session->record_init)(&tls_session->clean_out);

	/*
	 *	Read (and decrypt) the tunneled data from the SSL session,
	 *	and put it into the decrypted data buffer.
	 */
	err = SSL_read(tls_session->ssl, tls_session->clean_out.data,
		       sizeof(tls_session->clean_out.data));
	if (err < 0) {
		/*
		 *	FIXME: Call SSL_get_error() to see what went
		 *	wrong.
		 */
		radlog(L_INFO, "rlm_eap_ttls: SSL_read Error");
		return PW_AUTHENTICATION_REJECT;
	}

	t = (ttls_tunnel_t *) tls_session->opaque;

	/*
	 *	If there's no data, maybe this is an ACK to an
	 *	MS-CHAP2-Success.
	 */
	if (err == 0) {
		if (t->authenticated) {
			DEBUG2("  TTLS: Got ACK, and the user was already authenticated.");
			return PW_AUTHENTICATION_ACK;
		} /* else no session, no data, die. */

		/*
		 *	FIXME: Call SSL_get_error() to see what went
		 *	wrong.
		 */
		radlog(L_INFO, "rlm_eap_ttls: SSL_read Error");
		return PW_AUTHENTICATION_REJECT;
	}

	data_len = tls_session->clean_out.used = err;
	data = tls_session->clean_out.data;

#ifndef NDEBUG
	if (debug_flag > 2) {
		unsigned int i;

		for (i = 0; i < data_len; i++) {
			if ((i & 0x0f) == 0) printf("  TTLS tunnel data in %04x: ", i);

			printf("%02x ", data[i]);

			if ((i & 0x0f) == 0x0f) printf("\n");
		}
		if ((data_len & 0x0f) != 0) printf("\n");
	}
#endif

	if (!diameter_verify(data, data_len)) {
		return PW_AUTHENTICATION_REJECT;
	}

	/*
	 *	Allocate a fake REQUEST structe.
	 */
	fake = request_alloc_fake(request);

	rad_assert(fake->packet->vps == NULL);

	/*
	 *	Add the tunneled attributes to the fake request.
	 */
	fake->packet->vps = diameter2vp(tls_session->ssl, data, data_len);
	if (!fake->packet->vps) {
		return PW_AUTHENTICATION_REJECT;
	}

	/*
	 *	Tell the request that it's a fake one.
	 */
	vp = pairmake("Freeradius-Proxied-To", "127.0.0.1", T_OP_EQ);
	if (vp) {
		pairadd(&fake->packet->vps, vp);
	}

#ifndef NDEBUG
	if (debug_flag > 0) {
		printf("  TTLS: Got tunneled request\n");
		
		for (vp = fake->packet->vps; vp != NULL; vp = vp->next) {
			putchar('\t');vp_print(stdout, vp);putchar('\n');
		}
	}
#endif

	/*
	 *	Update other items in the REQUEST data structure.
	 */
	fake->username = pairfind(fake->packet->vps, PW_USER_NAME);
	fake->password = pairfind(fake->packet->vps, PW_PASSWORD);

	/*
	 *	No User-Name, try to create one from stored data.
	 */
	if (!fake->username) {
		/*
		 *	No User-Name in the stored data, look for
		 *	an EAP-Identity, and pull it out of there.
		 */
		if (!t->username) {
			vp = pairfind(fake->packet->vps, PW_EAP_MESSAGE);
			if (vp &&
			    (vp->length >= EAP_HEADER_LEN + 2) &&
			    (vp->strvalue[0] == PW_EAP_RESPONSE) &&
			    (vp->strvalue[EAP_HEADER_LEN] == PW_EAP_IDENTITY) &&
			    (vp->strvalue[EAP_HEADER_LEN + 1] != 0)) {
				/*
				 *	Create & remember a User-Name
				 */
				t->username = pairmake("User-Name", "", T_OP_EQ);
				rad_assert(t->username != NULL);

				memcpy(t->username->strvalue, vp->strvalue + 5,
				       vp->length - 5);
				t->username->length = vp->length - 5;
				t->username->strvalue[t->username->length] = 0;

				DEBUG2("  TTLS: Got tunneled identity of %s",
				       t->username->strvalue);

				/*
				 *	If there's a default EAP type,
				 *	set it here.
				 */
				if (t->default_eap_type != 0) {
					DEBUG2("  TTLS: Setting default EAP type for tunneled EAP session.");
					vp = paircreate(PW_EAP_TYPE,
							PW_TYPE_INTEGER);
					rad_assert(vp != NULL);
					vp->lvalue = t->default_eap_type;
					pairadd(&fake->config_items, vp);
				}

			} else {
				/*
				 *	Don't reject the request outright,
				 *	as it's permitted to do EAP without
				 *	user-name.
				 */
				DEBUG2("  rlm_eap_ttls: WARNING! No EAP-Identity found to start EAP conversation.");
			}
		} /* else there WAS a t->username */

		if (t->username) {
			vp = paircopy(t->username);
			pairadd(&fake->packet->vps, vp);
			fake->username = pairfind(fake->packet->vps, PW_USER_NAME);
		}
	} /* else the request ALREADY had a User-Name */

	/*
	 *	Add the State attribute, too, if it exists.
	 */
	if (t->state) {
		DEBUG2("  TTLS: Adding old state with %02x %02x",
		       t->state->strvalue[0], t->state->strvalue[1]);
		vp = paircopy(t->state);
		if (vp) pairadd(&fake->packet->vps, vp);
	}

	/*
	 *	If this is set, we copy SOME of the request attributes
	 *	from outside of the tunnel to inside of the tunnel.
	 *
	 *	We copy ONLY those attributes which do NOT already
	 *	exist in the tunneled request.
	 */
	if (t->copy_request_to_tunnel) {
		VALUE_PAIR *copy;

		for (vp = request->packet->vps; vp != NULL; vp = vp->next) {
			/*
			 *	The attribute is a server-side thingy,
			 *	don't copy it.
			 */
			if ((vp->attribute > 255) &&
			    (((vp->attribute >> 16) & 0xffff) == 0)) {
				continue;
			}

			/*
			 *	The outside attribute is already in the
			 *	tunnel, don't copy it.
			 *
			 *	This works for BOTH attributes which
			 *	are originally in the tunneled request,
			 *	AND attributes which are copied there
			 *	from below.
			 */
			if (pairfind(fake->packet->vps, vp->attribute)) {
				continue;
			}

			/*
			 *	Some attributes are handled specially.
			 */
			switch (vp->attribute) {
				/*
				 *	NEVER copy Message-Authenticator,
				 *	EAP-Message, or State.  They're
				 *	only for outside of the tunnel.
				 */
			case PW_USER_NAME:
			case PW_USER_PASSWORD:
			case PW_CHAP_PASSWORD:
			case PW_CHAP_CHALLENGE:
			case PW_PROXY_STATE:
			case PW_MESSAGE_AUTHENTICATOR:
			case PW_EAP_MESSAGE:
			case PW_STATE:
				continue;
				break;

				/*
				 *	By default, copy it over.
				 */
			default:
				break;
			}

			/*
			 *	Don't copy from the head, we've already
			 *	checked it.
			 */
			copy = paircopy2(vp, vp->attribute);
			pairadd(&fake->packet->vps, copy);
		}
	}

#ifndef NDEBUG
	if (debug_flag > 0) {
	  printf("  TTLS: Sending tunneled request\n");

	  for (vp = fake->packet->vps; vp != NULL; vp = vp->next) {
	    putchar('\t');vp_print(stdout, vp);putchar('\n');
	  }
	}
#endif

	/*
	 *	Call authentication recursively, which will
	 *	do PAP, CHAP, MS-CHAP, etc.
	 */
	rad_authenticate(fake);

	/*
	 *	Note that we don't do *anything* with the reply
	 *	attributes.
	 */
#ifndef NDEBUG
	if (debug_flag > 0) {
	  printf("  TTLS: Got tunneled reply RADIUS code %d\n",
		 fake->reply->code);

	  for (vp = fake->reply->vps; vp != NULL; vp = vp->next) {
	    putchar('\t');vp_print(stdout, vp);putchar('\n');
	  }
	}
#endif

	/*
	 *	Decide what to do with the reply.
	 */
	switch (fake->reply->code) {
	case 0:			/* No reply code, must be proxied... */
		vp = pairfind(fake->config_items, PW_PROXY_TO_REALM);
		if (vp) {
			eap_tunnel_data_t *tunnel;
			DEBUG2("  TTLS: Tunneled authentication will be proxied to %s", vp->strvalue);

			/*
			 *	Tell the original request that it's going
			 *	to be proxied.
			 */
			pairmove2(&(request->config_items),
				  &(fake->config_items),
				  PW_PROXY_TO_REALM);

			/*
			 *	Seed the proxy packet with the
			 *	tunneled request.
			 */
			rad_assert(request->proxy == NULL);
			request->proxy = fake->packet;
			fake->packet = NULL;
			rad_free(&fake->reply);
			fake->reply = NULL;

			/*
			 *	Set up the callbacks for the tunnel
			 */
			tunnel = rad_malloc(sizeof(*tunnel));
			memset(tunnel, 0, sizeof(*tunnel));

			tunnel->tls_session = tls_session;
			tunnel->callback = eapttls_postproxy;

			/*
			 *	Associate the callback with the request.
			 */
			rcode = request_data_add(request,
						 request->proxy,
						 REQUEST_DATA_EAP_TUNNEL_CALLBACK,
						 tunnel, free);
			rad_assert(rcode == 0);
			
			/*
			 *	rlm_eap.c has taken care of associating
			 *	the handler with the fake request.
			 *
			 *	So we associate the fake request with
			 *	this request.
			 */
			rcode = request_data_add(request,
						 request->proxy,
						 REQUEST_DATA_EAP_MSCHAP_TUNNEL_CALLBACK,
						 fake, my_request_free);
			rad_assert(rcode == 0);
			fake = NULL;

			/*
			 *	Didn't authenticate the packet, but
			 *	we're proxying it.
			 */
			rcode = PW_STATUS_CLIENT;

		} else {
			DEBUG2("  TTLS: No tunneled reply was found for request %d , and the request was not proxied: rejecting the user.",
			       request->number);
			rcode = PW_AUTHENTICATION_REJECT;
		}
		break;

	default:
		/*
		 *	Returns RLM_MODULE_FOO, and we want to return
		 *	PW_FOO
		 */
		rcode = process_reply(handler, tls_session, request,
				      fake->reply);
		switch (rcode) {
		case RLM_MODULE_REJECT:
			rcode = PW_AUTHENTICATION_REJECT;
			break;
			
		case RLM_MODULE_HANDLED:
			rcode = PW_ACCESS_CHALLENGE;
			break;
			
		case RLM_MODULE_OK:
			rcode = PW_AUTHENTICATION_ACK;
			break;
			
		default:
			rcode = PW_AUTHENTICATION_REJECT;
			break;
		}
		break;
	}

	request_free(&fake);

	return rcode;
}
