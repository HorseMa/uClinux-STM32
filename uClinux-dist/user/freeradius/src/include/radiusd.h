#ifndef RADIUSD_H
#define RADIUSD_H
/*
 * radiusd.h	Structures, prototypes and global variables
 *		for the FreeRADIUS server.
 *
 * Version:	$Id: radiusd.h,v 1.156.2.4 2005/04/11 23:45:21 aland Exp $
 *
 */
#include "libradius.h"
#include "radpaths.h"
#include "conf.h"
#include "conffile.h"

#include <stdarg.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_PTHREAD_H
#include	<pthread.h>
typedef pthread_t child_pid_t;
#define child_kill pthread_kill
#else
typedef pid_t child_pid_t;
#define child_kill kill
#endif

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#include "missing.h"

#define NO_SUCH_CHILD_PID (child_pid_t) (0)

#ifndef NDEBUG
#define REQUEST_MAGIC (0xdeadbeef)
#endif

/*
 *	See util.c
 */
typedef struct request_data_t request_data_t;

#define REQUEST_DATA_REGEX (0xadbeef00)
#define REQUEST_MAX_REGEX (8)

typedef struct auth_req {
#ifndef NDEBUG
	uint32_t		magic; /* for debugging only */
#endif
	RADIUS_PACKET		*packet;
	RADIUS_PACKET		*proxy;
	RADIUS_PACKET		*reply;
	RADIUS_PACKET		*proxy_reply;
	VALUE_PAIR		*config_items;
	VALUE_PAIR		*username;
	VALUE_PAIR		*password;
	request_data_t		*data;
	char			secret[32];
	child_pid_t    		child_pid;
	time_t			timestamp;
	int			number; /* internal server number */

	/*
	 *	We could almost keep a const char here instead of a
	 *	_copy_ of the secret... but what if the RADCLIENT
	 *	structure is freed because it was taken out of the
	 *	config file and SIGHUPed?
	 */
	char			proxysecret[32];
	int			proxy_try_count;
	int			proxy_outstanding;
	time_t			proxy_next_try;

	int                     simul_max;
	int                     simul_count;
	int                     simul_mpp; /* WEIRD: 1 is false, 2 is true */

	int			finished;
	int			options; /* miscellanous options */
	void			*container;
} REQUEST;

#define RAD_REQUEST_OPTION_NONE            (0)
#define RAD_REQUEST_OPTION_LOGGED_CHILD    (1 << 0)
#define RAD_REQUEST_OPTION_DELAYED_REJECT  (1 << 1)
#define RAD_REQUEST_OPTION_DONT_CACHE      (1 << 2)
#define RAD_REQUEST_OPTION_FAKE_REQUEST    (1 << 3)
#define RAD_REQUEST_OPTION_REJECTED        (1 << 4)
#define RAD_REQUEST_OPTION_PROXIED         (1 << 5)

/*
 *  Function handler for requests.
 */
typedef		int (*RAD_REQUEST_FUNP)(REQUEST *);

typedef struct radclient {
	uint32_t		ipaddr;
	uint32_t		netmask;
	char			longname[256];
	u_char			secret[32];
	char			shortname[32];
	char			nastype[32];
	char			login[32];
	char			password[32];
	struct radclient	*next;
} RADCLIENT;

typedef struct nas {
	uint32_t		ipaddr;
	char			longname[256];
	char			shortname[32];
	char			nastype[32];
	struct nas		*next;
} NAS;

typedef struct _realm {
	char			realm[64];
	char			server[64];
	char			acct_server[64];
	uint32_t		ipaddr;	/* authentication */
	uint32_t		acct_ipaddr;
	u_char			secret[32];
	time_t			last_reply; /* last time we saw a packet */
	int			auth_port;
	int			acct_port;
	int			striprealm;
	int			trusted; /* old */
	int			notrealm;
	int			active;	/* is it dead? */
	time_t			wakeup;	/* when we should try it again */
	int			acct_active;
	time_t			acct_wakeup;
	int			ldflag;
	struct _realm		*next;
} REALM;

typedef struct pair_list {
	char			*name;
	VALUE_PAIR		*check;
	VALUE_PAIR		*reply;
	int			lineno;
	struct pair_list	*next;
	struct pair_list	*lastdefault;
} PAIR_LIST;


/*
 *	Types of listeners.
 */
typedef enum RAD_LISTEN_TYPE {
	RAD_LISTEN_NONE = 0,
	RAD_LISTEN_AUTH,
	RAD_LISTEN_ACCT,
	RAD_LISTEN_PROXY
} RAD_LISTEN_TYPE;


/*
 *	For listening on multiple IP's and ports.
 */
typedef struct rad_listen_t {
	struct rad_listen_t *next; /* could be rbtree stuff */
	uint32_t	ipaddr;
	RAD_LISTEN_TYPE	type;
	int		port;
	int		fd;
} rad_listen_t;


typedef struct main_config_t {
	struct main_config *next;
	time_t		config_dead_time;
	uint32_t	myip;
	int		log_auth;
	int		log_auth_badpass;
	int		log_auth_goodpass;
	int		do_usercollide;
#ifdef WITH_SNMP
	int		do_snmp;
#endif
	int		allow_core_dumps;
	int		debug_level;
	int		proxy_requests;
	int		post_proxy_authorize;
	int		wake_all_if_all_dead;
	int		proxy_synchronous;
	int		proxy_dead_time;
	int		proxy_retry_count;
	int		proxy_retry_delay;
	int		proxy_fallback;
	int		reject_delay;
	int		status_server;
	int		max_request_time;
	int		cleanup_delay;
	int		max_requests;
	int		kill_unresponsive_children;
	char 		*do_lower_user;
	char		*do_lower_pass;
	char		*do_nospace_user;
	char		*do_nospace_pass;
	char		*nospace_time;
	char		*log_file;
	char		*checkrad;
	const char      *pid_file;
	const char	*uid_name;
	const char	*gid_name;
	rad_listen_t	*listen;
	CONF_SECTION	*config;
	RADCLIENT	*clients;
	REALM		*realms;
} MAIN_CONFIG_T;

#define DEBUG	if(debug_flag)log_debug
#define DEBUG2  if (debug_flag > 1)log_debug
#define DEBUG3  if (debug_flag > 2)log_debug

#define SECONDS_PER_DAY		86400
#define MAX_REQUEST_TIME	30
#define CLEANUP_DELAY		5
#define MAX_REQUESTS		256
#define RETRY_DELAY             5
#define RETRY_COUNT             3
#define DEAD_TIME               120

#define L_DBG			1
#define L_AUTH			2
#define L_INFO			3
#define L_ERR			4
#define L_PROXY			5
#define L_CONS			128

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
/*
 *	This definition of true as NOT false is definitive. :) Making
 *	it '1' can cause problems on stupid platforms.  See articles
 *	on C portability for more information.
 */
#define TRUE (!FALSE)
#endif

/* for paircompare_register */
typedef int (*RAD_COMPARE_FUNC)(void *instance, REQUEST *,VALUE_PAIR *, VALUE_PAIR *, VALUE_PAIR *, VALUE_PAIR **);

typedef enum radlog_dest_t {
  RADLOG_FILES = 0,
  RADLOG_SYSLOG,
  RADLOG_STDOUT,
  RADLOG_STDERR,
  RADLOG_NULL
} radlog_dest_t;

/*
 *	Global variables.
 *
 *	We really shouldn't have this many.
 */
extern const char	*progname;
extern int		debug_flag;
extern int		syslog_facility;
extern const char	*radacct_dir;
extern const char	*radlog_dir;
extern const char	*radlib_dir;
extern const char	*radius_dir;
extern const char	*radius_libdir;
extern radlog_dest_t	radlog_dest;
extern uint32_t		expiration_seconds;
extern int		log_stripped_names;
extern int		log_auth_detail;
extern int		acct_port;
extern const char      *radiusd_version;

/*
 *	Function prototypes.
 */

/* acct.c */
int		rad_accounting(REQUEST *);

/* session.c */
int		rad_check_ts(uint32_t nasaddr, unsigned int port, const char *user,
			     const char *sessionid);
int		session_zap(REQUEST *request, uint32_t nasaddr,
			    unsigned int port, const char *user,
			    const char *sessionid, uint32_t cliaddr,
			    char proto);

/* radiusd.c */
#ifndef _LIBRADIUS
void		debug_pair(FILE *, VALUE_PAIR *);
#endif
int		log_err (char *);
int		rad_respond(REQUEST *, RAD_REQUEST_FUNP fun);

/* util.c */
void (*reset_signal(int signo, void (*func)(int)))(int);
void		request_free(REQUEST **request);
int		rad_mkdir(char *directory, int mode);
int		rad_checkfilename(const char *filename);
void		*rad_malloc(size_t size); /* calls exit(1) on error! */
void		xfree(const char *ptr);
REQUEST		*request_alloc(void);
REQUEST		*request_alloc_fake(REQUEST *oldreq);
int		request_data_add(REQUEST *request,
				 void *unique_ptr, int unique_int,
				 void *opaque, void (*free_opaque)(void *));
void		*request_data_get(REQUEST *request,
				  void *unique_ptr, int unique_int);
void		request_reject(REQUEST *request);
void		rfc_clean(RADIUS_PACKET *packet);

/* client.c */
int		read_clients_file(const char *file);
RADCLIENT	*client_find(uint32_t ipno);
const char	*client_name(uint32_t ipno);
void		client_walk(void);
void		clients_free(RADCLIENT *cl);

/* files.c */
REALM		*realm_find(const char *, int);
REALM		*realm_findbyaddr(uint32_t ipno, int port);
void		realm_free(REALM *cl);
void		realm_disable(uint32_t ipno, int port);
int		pairlist_read(const char *file, PAIR_LIST **list, int complain);
void		pairlist_free(PAIR_LIST **);
int		read_config_files(void);
int		read_realms_file(const char *file);

/* nas.c */
int		read_naslist_file(char *);
NAS		*nas_find(uint32_t ipno);
const char	*nas_name(uint32_t ipno);
const char	*nas_name2(RADIUS_PACKET *r);
char  *		nas_name3(char *buf, size_t buflen, uint32_t ipno);
NAS		*nas_findbyname(char *nasname);

/* version.c */
void		version(void);

/* log.c */
int		vradlog(int, const char *, va_list ap);
int		radlog(int, const char *, ...)
#ifdef __GNUC__
		__attribute__ ((format (printf, 2, 3)))
#endif
;
int		log_debug(const char *, ...)
#ifdef __GNUC__
		__attribute__ ((format (printf, 1, 2)))
#endif
;
void 		vp_listdebug(VALUE_PAIR *vp);

/* proxy.c */
int proxy_receive(REQUEST *request);
int proxy_send(REQUEST *request);

/* auth.c */
char	*auth_name(char *buf, size_t buflen, REQUEST *request, int do_cli);
int		rad_authenticate (REQUEST *);
int		rad_check_password(REQUEST *request);
int		rad_postauth(REQUEST *);

/* exec.c */
int		radius_exec_program(const char *,  REQUEST *, int,
				    char *user_msg, int msg_len,
				    VALUE_PAIR *input_pairs,
				    VALUE_PAIR **output_pairs);

/* timestr.c */
int		timestr_match(char *, time_t);

/* valuepair.c */
int		paircompare_register(int attr, int otherattr,
				     RAD_COMPARE_FUNC func,
				     void *instance);
void		paircompare_unregister(int attr, RAD_COMPARE_FUNC func);
int		paircmp(REQUEST *req, VALUE_PAIR *request, VALUE_PAIR *check,
			VALUE_PAIR **reply);
int		simplepaircmp(REQUEST *, VALUE_PAIR *, VALUE_PAIR *);
void		pair_builtincompare_init(void);
void		pairxlatmove(REQUEST *, VALUE_PAIR **to, VALUE_PAIR **from);

/* xlat.c */
typedef int (*RADIUS_ESCAPE_STRING)(char *out, int outlen, const char *in);

int            radius_xlat(char * out, int outlen, const char *fmt,
			   REQUEST * request, RADIUS_ESCAPE_STRING func);
typedef int (*RAD_XLAT_FUNC)(void *instance, REQUEST *, char *, char *, size_t, RADIUS_ESCAPE_STRING func);
int		xlat_register(const char *module, RAD_XLAT_FUNC func, void *instance);
void		xlat_unregister(const char *module, RAD_XLAT_FUNC func);


/* threads.c */
extern		int thread_pool_init(void);
extern		int thread_pool_clean(time_t now);
extern		int thread_pool_addrequest(REQUEST *, RAD_REQUEST_FUNP);
extern		pid_t rad_fork(int exec_wait);
extern		pid_t rad_waitpid(pid_t pid, int *status, int options);
extern          int total_active_threads(void);

#ifndef HAVE_PTHREAD_H
#define rad_fork(n) fork()
#define rad_waitpid waitpid
#endif

/* mainconfig.h */
/* Define a global config structure */
extern struct main_config_t mainconfig;

int read_mainconfig(int reload);
int free_mainconfig(void);
CONF_SECTION *read_radius_conf_file(void); /* for radwho and friends. */
#endif /*RADIUSD_H*/
