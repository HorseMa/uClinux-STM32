#ifndef _RADIUS_SNMP_H
#define _RADIUS_SNMP_H
/*
 * Version:	$Id: radius_snmp.h,v 1.12.4.1 2006/02/07 15:29:10 nbk Exp $
 */
#ifdef WITH_SNMP

typedef enum smux_event_t {
  SMUX_NONE, SMUX_CONNECT, SMUX_READ
} smux_event_t;

extern void radius_snmp_init(void);
extern int smux_connect(void);
extern int smux_read(void);

/*
 *	The RADIUS server snmp data structures.
 */
typedef struct rad_snmp_server_t {
	const char 	*ident;
	time_t		start_time;
	int32_t		uptime;	/* in hundredths of a second */

	time_t		last_reset_time;
	int32_t		reset_time;
	int32_t		config_reset;
	int32_t		total_requests;
	int32_t		total_invalid_requests;
	int32_t		total_dup_requests;
	int32_t		total_responses;
	int32_t		total_access_accepts;
	int32_t		total_access_rejects;
	int32_t		total_access_challenges;
	int32_t		total_malformed_requests;
	int32_t		total_bad_authenticators;
	int32_t		total_packets_dropped;
	int32_t		total_no_records;
	int32_t		total_unknown_types;
} rad_snmp_server_t;

typedef struct rad_snmp_t {
	rad_snmp_server_t auth;
	rad_snmp_server_t acct;
	smux_event_t      smux_event;
	const char	  *smux_password;
	int		  snmp_write_access;
	int		  smux_fd;
	int		  smux_failures;
	int		  smux_max_failures;
} rad_snmp_t;

/*
 *  Taken from RFC 2619 and RFC 2621
 */
typedef struct rad_snmp_client_entry_t {
	int		index;
	/* IP address */
	/* Client ID (string ) */
	int		access_requests;
	int		dup_access_requests;
	int		access_accepts;
	int		access_rejects;
	int		access_challenges;
	int		auth_malformed_requests;
	int		auth_bad_authenticators;
	int		auth_packets_dropped;
	int		auth_unknown_types;
	int		acct_packets_dropped;
	int		acct_requests;
	int		acct_dup_requests;
	int		acct_responses;
	int		acct_bad_authenticators;
	int		acct_malformed_requests;
	int		acct_no_records;
	int		acct_unknown_types;
} rad_snmp_client_entry_t;

extern rad_snmp_t	rad_snmp;

#define RAD_SNMP_INC(_x) if (mainconfig.do_snmp) _x++
#define RAD_SNMP_TYPE_INC(_listener, _x) if (mainconfig.do_snmp) { \
                                     if (_listener->type == RAD_LISTEN_AUTH) { \
                                       rad_snmp.auth._x++; \
				     } else { if (_listener->type == RAD_LISTEN_ACCT) \
                                       rad_snmp.acct._x++; } } \


#else
#define  RAD_SNMP_INC(_x)
#define RAD_SNMP_TYPE_INC(_listener, _x)

#endif /* WITH_SNMP */

#endif /* _RADIUS_SNMP_H */
